#include "senderThread.h"
#include "log.h"
#include "libyuv.h"

#define DEBUG_NV21_TIMING               0
#define DEBUG_ARGB_TIMING               0
#define DEBUG_RENDERING_TIMING          0

void senderThread::sizeChanged(int width,int height){
    if(nv21_array != NULL){
        free(nv21_array);
        nv21_array = NULL;
    }
    if(argb_array != NULL){
        free(argb_array);
        argb_array = NULL;
    }
    argb_size = static_cast<size_t>(width * height * 4);
    nv21_size = static_cast<size_t>(width * height * 1.5);

    argb_array = static_cast<uint8_t *>(malloc(argb_size));
    nv21_array = static_cast<uint8_t *>(malloc(nv21_size));
}

void senderThread::getStride(int width,int height,int colorFormat,/*input*/
               int &yStride,int &uvStride,int &ySpan,int &uvSpan,int &yuvType) {/*out*/
    ySpan = height;
    uvSpan= height;
    switch (colorFormat){
        case 19:
            yuvType = 2;
            yStride = width;
            ySpan   = height;
            uvStride= width/2;
            uvSpan  = ySpan;
            break;
        case 21:
            yuvType = 0;
            yStride = width;
            ySpan   = height;
            uvStride= width;
            uvSpan  = ySpan/2;
            break;
        case 0x7FA30C03: {
            yuvType = 1;
            int hAlign = 64;
            int vAlign = 32;
            yStride = width % hAlign > 0 ? hAlign * (width / hAlign + 1) : width;
            ySpan = height % vAlign > 0 ? vAlign * (height / vAlign + 1) : height;
            uvStride = yStride;
            uvSpan = ySpan / 2;
        }
            break;
        case 0x7FA30C04:
        default:{
            yuvType = 0;
            int hAlign = 128;
            int vAlign = 32;
            yStride = width % hAlign > 0 ? hAlign * (width / hAlign + 1) : width;
            ySpan = height % vAlign > 0 ? vAlign * (height / vAlign + 1) : height;
            uvStride = yStride;
            uvSpan = ySpan / 2;
        }
            break;
    }
}

int senderThread::toARGB(FramePacket &p){
#if DEBUG_ARGB_TIMING
    struct timeval start,end;
    gettimeofday(&start,NULL);
#endif

    int yStride,uvStride,ySpan,uvSpan,yuvType;
    getStride(p.width,p.height,p.colorFormat,yStride,uvStride,ySpan,uvSpan,yuvType);
    int result = -1;
    uint8 *src_y = p.data;;
    uint8 *src_uv= p.data + yStride * ySpan;
    uint8_t *src_v= src_uv + uvStride * uvSpan / 2 ; //used for I420
    int argb_stride = p.width * 4;
    switch (yuvType){
        case 0://nv21
//            LOGI("crash info width %d,height %d,rgb stride %d,y stride %d,uv stride,argb size %d,")
            result = libyuv::NV21ToARGB(src_y,yStride,src_uv,uvStride,argb_array,argb_stride,p.width,p.height);
            break;
        case 1://nv12
            result = libyuv::NV12ToARGB(src_y,yStride,src_uv,uvStride,argb_array,argb_stride,p.width,p.height);
            break;
        case 2://I420  YV12
            result = libyuv::I420ToARGB(src_y,yStride,src_v,uvStride,src_uv,uvStride,argb_array,argb_stride,p.width,p.height);
            break;
        default:
            LOGE("uknown case");
            break;
    }
#if DEBUG_ARGB_TIMING
    gettimeofday(&end,NULL);
    long timeuse = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec
                   - start.tv_usec;
    LOGI("argb convert using %ld,yuv type %d",timeuse,yuvType);
#endif
    return result;
}
int senderThread::toNV21(FramePacket &p){
#if DEBUG_NV21_TIMING
    struct timeval start,end;
    gettimeofday(&start,NULL);
#endif

    int yStride,uvStride,ySpan,uvSpan,yuvType;
    getStride(p.width,p.height,p.colorFormat,yStride,uvStride,ySpan,uvSpan,yuvType);
    int result;
    uint8 *src_y = p.data;
    uint8 *src_uv= p.data + yStride * ySpan;
    uint8_t *src_v= src_uv + uvStride * uvSpan / 2 ; //used for I420
    uint8_t *dst_y = nv21_array;
    uint8_t *dst_vu= nv21_array+yStride*ySpan;
    switch(yuvType){
        case 2: //I420 YV12
            result = libyuv::I420ToNV21(src_y,yStride,src_v,uvStride,src_uv,uvStride,dst_y,yStride,dst_vu,p.width,p.width,p.height);
            break;
        case 0:
        case 1:
        default:
            result = 0;
            memcpy(nv21_array,p.data,p.length);
    }
#if DEBUG_NV21_TIMING
    gettimeofday(&end,NULL);
    long timeuse = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec
                   - start.tv_usec;
    LOGI("nv21 convert using %ld,yuvType %d,result %d,nv21 size %d",timeuse,yuvType,result,p.length);
#endif
    return result;
}

senderThread::senderThread(mediaCodecCallback *cb){
    int result = 0;
    result = pthread_create(&tid,NULL,workThread,this);
    isRunning = true;
    if(result != 0){
        LOGE("senderThread create failed");
        tid = -1;
    }
    callback = cb;
    pthread_mutex_init(&queueLock,NULL);

    nv21_array = NULL;
    argb_array = NULL;
}
senderThread::~senderThread() {
    isRunning = false;
    if(tid != -1){
        pthread_join(tid,NULL);
        tid = -1;
    }
    pthread_mutex_destroy(&queueLock);

    if(nv21_array != NULL){
        free(nv21_array);
        nv21_array = NULL;
    }
    if(argb_array != NULL){
        free(argb_array);
        argb_array = NULL;
    }
}
void* senderThread::workThread(void *arg) {
    senderThread *p = (senderThread *)arg;
    while (p->isRunning){
        if(!p->frameQueue.empty()){
            struct timeval now;
            gettimeofday(&now,NULL);
            FramePacket pkt = p->frameQueue.front();
            long timeuse = 1000000 * (now.tv_sec - pkt.timestamp.tv_sec) + now.tv_usec
                           - pkt.timestamp.tv_usec;
            if(timeuse > 18000){
                pthread_mutex_lock(&p->queueLock);
                p->frameQueue.pop();
                pthread_mutex_unlock(&p->queueLock);
//                LOGI("drop frame %ld",timeuse);
                if(p->callback != NULL && p->callback->frameDropped != NULL){
                    p->callback->frameDropped();
                }
                continue;
            }
            if(pkt.nv21notify){
                if(p->toNV21(pkt) == 0){
                    if(p->callback != NULL && p->callback->nv21Callback != NULL){
                        p->callback->nv21Callback(p->nv21_array,p->nv21_size,pkt.width,pkt.height);
                    }
                }
            }else if(pkt.drawByUser){
                if(p->toARGB(pkt) == 0){
                    if(p->callback != NULL && p->callback->argbCallback != NULL){
                        p->callback->argbCallback(p->argb_array,p->argb_size,pkt.width,pkt.height);
                    }
                }
            }
            pthread_mutex_lock(&p->queueLock);
            p->frameQueue.pop();
            pthread_mutex_unlock(&p->queueLock);

#if DEBUG_RENDERING_TIMING
            struct timeval end;
            gettimeofday(&end,NULL);
            long timeuse = 1000000 * (end.tv_sec - now.tv_sec) + end.tv_usec
                           - now.tv_usec;
            LOGI("rendering using %ld",timeuse);
#endif
            if(p->callback != NULL && p->callback->frameDropped != NULL){
                p->callback->frameRendered();
            }
        }
    }
    return NULL;
}

void senderThread::enqueueFrame(FramePacket &p){
    pthread_mutex_lock(&queueLock);
    frameQueue.push(p);
    pthread_mutex_unlock(&queueLock);
}