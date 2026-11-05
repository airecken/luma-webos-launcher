/* Exercise real C9 glue and SMP lifecycle against a deterministic fake decoder.
 * The TV test covers the real vendor libraries and visual playback.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <AcbAPI.h>
#include "smp_api.h"
void *luma_hw_open(const char *, int, int, const char *);
void luma_hw_close(void *);
int luma_hw_feed(void *, const void *, size_t, int64_t);
int64_t luma_hw_frames_rendered(void *);
int luma_hw_ended(void *);
void luma_hw_eos(void *);
struct smp_api { smp_event_fn *callback; void *user; };
static struct smp_api decoder;
static int state, finalized, destroyed, feeds;
static smp_feed_result result = SMP_FEED_OK;
static int64_t last_pts;
long AcbAPI_create(void) { return 42; }
bool AcbAPI_initialize(long id, AcbAPI_PlayerType type, const char *app, AcbAPI_Callback cb) {
    (void)cb; assert(id == 42 && type == PLAYER_TYPE_MSE && !strcmp(app, "test.luma")); return true;
}
bool AcbAPI_setMediaId(long id, const char *media) { assert(id == 42 && !strcmp(media,"decoder")); return true; }
bool AcbAPI_setSinkType(long id, AcbAPI_SinkType sink) { assert(id == 42 && sink == SINK_TYPE_MAIN); return true; }
int AcbAPI_setState(long id, AcbAPI_AppState app, AcbAPI_PlayState play, long *task) {
    assert(id == 42 && app == APPSTATE_FOREGROUND && task);
    assert(play != PLAYSTATE_PLAYING || state == PLAYSTATE_LOADED);
    state = play; return 1;
}
int AcbAPI_setDisplayWindow(long id, long x, long y, long w, long h, bool full, long *task) {
    assert(id == 42 && x == 0 && y == 0 && w == 1920 && h == 1080 && full && task); return 1;
}
int fake_video_data(long, const char *, long *) __asm__("AcbAPI_setMediaVideoData");
int fake_video_data(long id, const char *data, long *task) { assert(id == 42 && data && task); return 1; }
int AcbAPI_finalize(long id) { assert(id == 42 && state == PLAYSTATE_UNLOADED); finalized++; return 1; }
void AcbAPI_destroy(long id) { assert(id == 42); destroyed++; }
smp_api *smp_api_create(const char *uid) { (void)uid; return &decoder; }
void smp_api_destroy(smp_api *api) { assert(api == &decoder); }
const char *smp_api_media_id(smp_api *api) { assert(api == &decoder); return "decoder"; }
bool smp_api_notify_foreground(smp_api *api) { assert(api == &decoder); return true; }
bool smp_api_load(smp_api *api, const char *payload, smp_event_fn *cb, void *user) {
    assert(strstr(payload,"BUFFERSTREAM") && strstr(payload,"\"hasAudio\"") == NULL);
    api->callback = cb; api->user = user; return true;
}
bool smp_api_play(smp_api *api) { assert(api == &decoder); return true; }
bool smp_api_unload(smp_api *api) { assert(api == &decoder); return true; }
bool smp_api_push_eos(smp_api *api) { api->callback(SMP_EVENT_STR_STATE_UPDATE_ENDOFSTREAM,0,NULL,api->user); return true; }
const char *smp_api_last_feed_reply(const smp_api *api) { (void)api; return "fake"; }
smp_feed_result smp_api_feed(smp_api *api, const char *payload) {
    const char *pts = strstr(payload,"\"pts\":"); assert(pts);
    long long value; assert(sscanf(pts,"\"pts\":%lld", &value) == 1); last_pts = value;
    if (result == SMP_FEED_OK) {
        feeds++;
        /* First Feed precedes LOADCOMPLETED: ACB must hold PLAYING until LOADED. */
        if (feeds == 2) api->callback(SMP_EVENT_STR_STATE_UPDATE_LOADCOMPLETED,0,NULL,api->user);
        api->callback(SMP_EVENT_FRAMEREADY,last_pts,NULL,api->user);
    }
    return result;
}
int main(void) {
    assert(!luma_hw_open("test.luma",1920,1080,"H265"));
    void *player = luma_hw_open("test.luma",1920,1080,"H264"); assert(player);
    char data = 1;
    assert(luma_hw_feed(player,&data,1,66666666) == 0);
    assert(state == PLAYSTATE_UNLOADED);
    assert(luma_hw_feed(player,&data,1,33333333) == 0);
    assert(state == PLAYSTATE_PLAYING && last_pts == 33333333);
    assert(luma_hw_frames_rendered(player) == 2);
    result = SMP_FEED_BUFFER_FULL; assert(luma_hw_feed(player,&data,1,99999999) == 1);
    assert(luma_hw_frames_rendered(player) == 2);
    assert(!luma_hw_ended(player)); luma_hw_eos(player); assert(luma_hw_ended(player));
    luma_hw_close(player); assert(finalized == 1 && destroyed == 1);
    puts("C9 clocked lifecycle, display PTS, backpressure, EOS and cleanup passed");
}
