/* Clocked C9/webOS 4 hardware wallpaper. ACB places the SMP video plane
 * beneath Luma's transparent UI. Unlike NDL v1, SMP schedules source PTS.
 * ACB sequencing follows webosbrew/samples media/smp/acb (webOS 3/4).
 */
#include <AcbAPI.h>
#include "vendor/smp_player.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Retail firmware has a third task-id argument missing from the SDK header. */
extern int acb_video_data(long, const char *, long *) __asm__("AcbAPI_setMediaVideoData");
typedef struct {
    smp_player *player;
    long acb, task;
    pthread_mutex_t lock;
    int loaded, play_pending, failed;
    unsigned long fed, previous_frames;
} Player;
static void note(const char *event, long value) {
    const char *path = "/media/developer/com.denisefremov.luma/luma-c9-clocked.log";
    struct stat st;
    if (!stat(path, &st) && st.st_size > 1024 * 1024)
        rename(path, "/media/developer/com.denisefremov.luma/luma-c9-clocked.prev.log");
    FILE *f = fopen(path, "a");
    if (f) { fprintf(f, "%s %ld\n", event, value); fclose(f); }
}
static void acb_callback(long id, long task, long event, long app, long play, const char *reply) {
    (void)id; (void)task; (void)app; (void)play;
    note("ACB_EVENT", event);
    /* Replies contain placement/state diagnostics, never stream URLs. */
    if (reply && strstr(reply, "errorCode")) {
        FILE *f = fopen("/media/developer/com.denisefremov.luma/luma-c9-clocked.log", "a");
        if (f) { fprintf(f, "ACB_REPLY %.512s\n", reply); fclose(f); }
    }
}
static void media_id(void *opaque, const char *id) {
    Player *p = opaque;
    if (!id || !AcbAPI_setMediaId(p->acb, id)) p->failed = 1;
}
static void post_load(void *opaque, const smp_load_params *params) {
    (void)params; Player *p = opaque;
    note("DISPLAY", AcbAPI_setDisplayWindow(p->acb, 0, 0, 1920, 1080, true, &p->task));
}
static void loaded(void *opaque, const char *id) {
    (void)id; Player *p = opaque;
    AcbAPI_setSinkType(p->acb, SINK_TYPE_MAIN);
    note("LOADED", AcbAPI_setState(p->acb, APPSTATE_FOREGROUND, PLAYSTATE_LOADED, &p->task));
    pthread_mutex_lock(&p->lock);
    p->loaded = 1;
    int play = p->play_pending;
    p->play_pending = 0;
    pthread_mutex_unlock(&p->lock);
    if (play) note("PLAYING", AcbAPI_setState(p->acb, APPSTATE_FOREGROUND, PLAYSTATE_PLAYING, &p->task));
}
static void playing(void *opaque) {
    Player *p = opaque;
    pthread_mutex_lock(&p->lock);
    int ready = p->loaded;
    if (!ready) p->play_pending = 1;
    pthread_mutex_unlock(&p->lock);
    if (ready) note("PLAYING", AcbAPI_setState(p->acb, APPSTATE_FOREGROUND, PLAYSTATE_PLAYING, &p->task));
}
static void video_info(void *opaque, const char *info) {
    Player *p = opaque;
    if (info) note("VIDEO_INFO", acb_video_data(p->acb, info, &p->task));
}
static void unloaded(void *opaque) {
    Player *p = opaque;
    AcbAPI_setState(p->acb, APPSTATE_FOREGROUND, PLAYSTATE_UNLOADED, &p->task);
}
void luma_hw_close(void *opaque) {
    Player *p = opaque;
    if (!p) return;
    if (p->player) { smp_player_unload(p->player); smp_player_destroy(p->player); }
    if (p->acb) { AcbAPI_finalize(p->acb); AcbAPI_destroy(p->acb); }
    pthread_mutex_destroy(&p->lock);
    note("CLOSE", p->fed);
    free(p);
}
void *luma_hw_open(const char *app, int width, int height, const char *codec) {
    if (!app || width <= 0 || height <= 0 || !codec || strcmp(codec, "H264")) return NULL;
    Player *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    pthread_mutex_init(&p->lock, NULL);
    p->acb = AcbAPI_create();
    if (!p->acb || !AcbAPI_initialize(p->acb, PLAYER_TYPE_MSE, app, acb_callback)) goto fail;
    smp_video_plane plane = {0};
    plane.self = p; plane.set_media_id = media_id; plane.post_load = post_load;
    plane.load_completed = loaded; plane.start_playing = playing;
    plane.video_info = video_info; plane.post_unload = unloaded;
    p->player = smp_player_create(&plane);
    if (!p->player) goto fail;
    smp_load_params params = {0};
    params.app_id = app; params.has_video = true; params.video_codec = "H264";
    params.video_width = width; params.video_height = height;
    params.video_fps_num = 30; params.video_fps_den = 1;
    params.pause_at_decode_time = true; params.low_latency = false;
    if (!smp_player_load(p->player, &params) || p->failed) goto fail;
    note("OPEN_CLOCKED", 1);
    return p;
fail:
    note("OPEN_FAILED", 1); luma_hw_close(p); return NULL;
}
int luma_hw_feed(void *opaque, const void *data, size_t size, int64_t pts) {
    Player *p = opaque;
    if (!p || !p->player || !data || !size) return -1;
    smp_feed_result result = smp_player_feed(p->player, data, size, pts, SMP_ES_VIDEO);
    if (result == SMP_FEED_OK) { p->fed++; return 0; }
    return result == SMP_FEED_BUFFER_FULL || result == SMP_FEED_PENDING ? 1 : -1;
}
int64_t luma_hw_frames_rendered(void *opaque) {
    Player *p = opaque; return p ? smp_player_frames_rendered(p->player) : 0;
}
int luma_hw_ended(void *opaque) {
    Player *p = opaque;
    return !p || smp_player_ended(p->player) || smp_player_errored(p->player);
}
int luma_hw_clocked(void) { return 1; }
void luma_hw_eos(void *opaque) { Player *p = opaque; if (p) smp_player_push_eos(p->player); }
int luma_hw_set_drop_threshold(void *opaque, int n) { (void)opaque; (void)n; return 0; }
void luma_hw_metrics(void *opaque, int index, const char *source, const char *codec,
                     unsigned au, unsigned retries, unsigned errors,
                     double average, double maximum, size_t size) {
    (void)index; (void)source; (void)codec; (void)average; (void)maximum; (void)size;
    Player *p = opaque; if (!p) return;
    unsigned long frames = smp_player_frames_rendered(p->player);
    char line[160];
    snprintf(line, sizeof(line), "PACE au=%u retry=%u error=%u frames=%lu drops=%lld",
             au, retries, errors, frames - p->previous_frames,
             (long long)smp_player_frames_dropped(p->player));
    note(line, 0); p->previous_frames = frames;
}
