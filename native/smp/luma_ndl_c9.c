/* C9/webOS 4 NDL v1 adapter for the fork's hardware wallpaper ABI.
 * Uses hardware H.264 playback and completion callbacks, no software decode.
 */
#define NDL_DIRECTMEDIA_API_VERSION 1
#include <NDL_directmedia.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
typedef struct {void *data;size_t bytes;int64_t pts;} Packet;
typedef struct { int initialized,opened,error,stop,thread_started; unsigned long fed,frames; int64_t last_feed,last_frame; long feed_min,feed_max,frame_min,frame_max; unsigned long last_frames; pthread_t thread;pthread_mutex_t lock;pthread_cond_t condition;Packet queue[60];unsigned head,count;size_t queued_bytes; } Player;
static int64_t mono_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000000+t.tv_nsec;}
static void range(long *minimum,long *maximum,long value){
 long old=__atomic_load_n(minimum,__ATOMIC_RELAXED);
 while((!old||value<old)&&!__atomic_compare_exchange_n(minimum,&old,value,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){}
 old=__atomic_load_n(maximum,__ATOMIC_RELAXED);
 while(value>old&&!__atomic_compare_exchange_n(maximum,&old,value,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){}
}
static Player *active;
static void record(const char *event,long n) {
 FILE *f=fopen("/media/developer/com.denisefremov.luma/luma-c9-video.log","a");
 if(f){fprintf(f,"%s %ld\n",event,n);fclose(f);}
}
static void released(const char *reason){(void)reason;if(active)__atomic_store_n(&active->error,1,__ATOMIC_RELEASE);record("RESOURCE_RELEASED",0);}
static void rendered(unsigned long long cookie){
 (void)cookie;
 if(active){int64_t now=mono_ns(),previous=__atomic_exchange_n(&active->last_frame,now,__ATOMIC_RELAXED);if(previous)range(&active->frame_min,&active->frame_max,(long)((now-previous)/1000));unsigned long n=__atomic_add_fetch(&active->frames,1,__ATOMIC_RELEASE);if(n==1||n%300==0)record("FRAME_COMPLETED",n);}
}
static void *pump(void *opaque){
 Player *p=opaque;int64_t origin=0,first_pts=0;
 for(;;){
  pthread_mutex_lock(&p->lock);
  while(!p->count&&!__atomic_load_n(&p->stop,__ATOMIC_ACQUIRE))pthread_cond_wait(&p->condition,&p->lock);
  if(__atomic_load_n(&p->stop,__ATOMIC_ACQUIRE)){pthread_mutex_unlock(&p->lock);break;}
  Packet packet=p->queue[p->head];p->head=(p->head+1)%60;p->count--;p->queued_bytes-=packet.bytes;
  pthread_mutex_unlock(&p->lock);
  if(!origin){origin=mono_ns()+200000000;first_pts=packet.pts;}
  int64_t deadline=origin+(packet.pts-first_pts);
  while(!__atomic_load_n(&p->stop,__ATOMIC_ACQUIRE)&&mono_ns()<deadline){
   int64_t remaining=deadline-mono_ns();struct timespec delay={0,remaining>5000000?5000000:remaining};if(delay.tv_nsec>0)nanosleep(&delay,NULL);
  }
  int r=-1;
  if(!__atomic_load_n(&p->stop,__ATOMIC_ACQUIRE)){
   for(unsigned retry=0;retry<100&&!__atomic_load_n(&p->stop,__ATOMIC_ACQUIRE);retry++){
    r=NDL_DirectVideoPlayWithCallback(packet.data,(unsigned)packet.bytes,(unsigned long long)packet.pts);
    if(!r)break;usleep(1000);
   }
   if(r){__atomic_store_n(&p->error,1,__ATOMIC_RELEASE);record("SUBMIT_FAILED",r);}
   else{
    int64_t now=mono_ns(),previous=__atomic_exchange_n(&p->last_feed,now,__ATOMIC_RELAXED);
    if(previous)range(&p->feed_min,&p->feed_max,(long)((now-previous)/1000));
    unsigned long n=__atomic_add_fetch(&p->fed,1,__ATOMIC_RELEASE);if(n==1||n%300==0)record("FEED_OK",n);
   }
  }
  free(packet.data);
 }
 return NULL;
}
void *luma_hw_open(const char *app,int width,int height,const char *codec){
 if(!app||active||width<=0||height<=0)return NULL;
 if(codec&&strcmp(codec,"H264")){record("UNSUPPORTED_CODEC",0);return NULL;}
 Player *p=calloc(1,sizeof(*p));if(!p)return NULL;active=p;
 int r=NDL_DirectMediaInit(app,released);record("INIT",r);if(r)goto fail;p->initialized=1;
 r=NDL_DirectMediaSetAppState(NDL_DIRECTMEDIA_APP_STATE_FOREGROUND);record("FOREGROUND",r);
 NDL_DIRECTVIDEO_DATA_INFO_T v={.width=width,.height=height,.source=NDL_DIRECTVIDEO_SRC_TYPE_H264};
 r=NDL_DirectVideoOpen(&v);record("VIDEO_OPEN",r);if(r)goto fail;p->opened=1;
 r=NDL_DirectVideoSetCallback(rendered);record("CALLBACK",r);if(r)goto fail;
 r=NDL_DirectVideoSetArea(0,0,1920,1080);record("AREA",r);
 pthread_mutex_init(&p->lock,NULL);pthread_cond_init(&p->condition,NULL);
 r=pthread_create(&p->thread,NULL,pump,p);if(r){pthread_mutex_destroy(&p->lock);pthread_cond_destroy(&p->condition);goto fail;}p->thread_started=1;
 return p;
fail:
 if(p->opened)NDL_DirectVideoClose();if(p->initialized)NDL_DirectMediaQuit();active=NULL;free(p);return NULL;
}
int luma_hw_feed(void *ptr,const void *buffer,size_t size,int64_t pts){
 Player *p=ptr;if(!p||!p->opened||!buffer||!size||size>UINT_MAX||__atomic_load_n(&p->error,__ATOMIC_ACQUIRE))return -1;
 pthread_mutex_lock(&p->lock);
 if(p->count==60||p->queued_bytes+size>16*1024*1024){pthread_mutex_unlock(&p->lock);return 1;}
 void *data=malloc(size);if(!data){pthread_mutex_unlock(&p->lock);return -1;}memcpy(data,buffer,size);
 p->queue[(p->head+p->count)%60]=(Packet){data,size,pts};p->count++;p->queued_bytes+=size;
 pthread_cond_signal(&p->condition);pthread_mutex_unlock(&p->lock);return 0;
}
int64_t luma_hw_frames_rendered(void *ptr){Player *p=ptr;return p?(int64_t)__atomic_load_n(&p->frames,__ATOMIC_ACQUIRE):0;}
int luma_hw_ended(void *ptr){Player *p=ptr;return !p||__atomic_load_n(&p->error,__ATOMIC_ACQUIRE);}
int luma_hw_set_drop_threshold(void *ptr,int n){(void)ptr;(void)n;return 0;}
void luma_hw_metrics(void *ptr,int index,const char *source,const char *codec,unsigned au,unsigned retries,unsigned errors,double average,double maximum,size_t size){
 (void)index;(void)source;(void)codec;(void)size;Player *p=ptr;if(!p)return;
 const char *path="/media/developer/com.denisefremov.luma/luma-c9-pacing.log";struct stat st;if(!stat(path,&st)&&st.st_size>1024*1024)rename(path,"/media/developer/com.denisefremov.luma/luma-c9-pacing.prev.log");
 FILE *f=fopen(path,"a");if(!f)return;unsigned long frames=__atomic_load_n(&p->frames,__ATOMIC_ACQUIRE);
 fprintf(f,"au=%u retry=%u error=%u late_avg_ms=%.2f late_max_ms=%.2f feed_us=%ld..%ld frames=%lu frame_us=%ld..%ld\n",au,retries,errors,average,maximum,__atomic_exchange_n(&p->feed_min,0,__ATOMIC_RELAXED),__atomic_exchange_n(&p->feed_max,0,__ATOMIC_RELAXED),frames-p->last_frames,__atomic_exchange_n(&p->frame_min,0,__ATOMIC_RELAXED),__atomic_exchange_n(&p->frame_max,0,__ATOMIC_RELAXED));fclose(f);
 p->last_frames=frames;
}
void luma_hw_close(void *ptr){
 Player *p=ptr;if(!p)return;
 if(p->thread_started){pthread_mutex_lock(&p->lock);__atomic_store_n(&p->stop,1,__ATOMIC_RELEASE);pthread_cond_signal(&p->condition);pthread_mutex_unlock(&p->lock);pthread_join(p->thread,NULL);while(p->count){free(p->queue[p->head].data);p->head=(p->head+1)%60;p->count--;}pthread_mutex_destroy(&p->lock);pthread_cond_destroy(&p->condition);}
 if(p->opened){NDL_DirectVideoStop();NDL_DirectVideoClose();}if(p->initialized)NDL_DirectMediaQuit();
 active=NULL;record("CLOSE",p->fed);free(p);
}
