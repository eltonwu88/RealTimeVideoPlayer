#include <climits>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <cstdlib>
#include <pthread.h>
#include <queue>
#include <jni.h>
#include <android/native_window_jni.h>
#include "log.h"
#include "libyuv.h"
#include "mediacodec.h"
#include "senderThread.h"

using namespace std;

static unsigned char SPS[] = {0x00,0x00,0x01,0x67,0x4d,0x00,0x1f,0xe5,0x40,0x28,0x02,0xd8,0x80};
static unsigned char PPS[] = {0x00,0x00,0x01,0x68,0xee,0x31,0x12};

static const char *MIME_TYPE = "video/avc";
static const char *CSD0 = "csd-0";
static const char *CSD1 = "csd-1";

static unsigned char *spsBuffer = NULL;
static unsigned char *ppsBuffer = NULL;

static size_t spsBuffer_size;
static size_t ppsBuffer_size;

static int width;
static int height;

static AMediaCodec   *mediaCodec    = NULL;
static ANativeWindow *videoSurface  = NULL;

static bool notify_nv21 = false;
static bool draw_by_user= false;
static mediaCodecCallback *mCallback;

int output_colorFormat;

volatile bool droppingMode = false;

static senderThread *_sender = NULL;

class VideoPacket {
public :
    VideoPacket(unsigned char *packet,size_t len,bool keyframe){
        buffer = static_cast<uint8_t *>(malloc(len));
        this->len = len;
        if(buffer == NULL){
            LOGE("failed to alloc packet");
            return;
        }
        memcpy(buffer,packet,len);
        gettimeofday(&timestamp,NULL);
        isKeyFrame = keyframe;
    }
    ~VideoPacket(){
        if(buffer != NULL){
            free(buffer);
            buffer = NULL;
        }
    }
    VideoPacket(const VideoPacket &p){
        buffer = static_cast<uint8_t *>(malloc(p.len));
        this->len = p.len;
        if(buffer == NULL){
            LOGE("failed to alloc packet");
            return;
        }
        memcpy(buffer,p.buffer,len);
        this->timestamp = p.timestamp;
        this->isKeyFrame= p.isKeyFrame;
    }

    uint8_t *buffer;
    size_t  len;
    struct timeval timestamp;
    bool    isKeyFrame;
};

static queue<VideoPacket> dataQueue;
static pthread_mutex_t queueLock;

static volatile bool isDecodeThreadRunning = false;
static pthread_t decode_tid = -1;
static int numPkts;

static void clearQueue(queue<VideoPacket> *queue){
    LOGD("clearing queue");
    while(!queue->empty()){
        queue->pop();
    }
    LOGD("clear queue finished");
}
// -1 drop directly,
// 0  ok
//1   key frame do not drop
int shouldDropFrame(){
    struct timeval now;
    int result = 0;
    pthread_mutex_lock(&queueLock);
    VideoPacket pkt = dataQueue.front();
    gettimeofday(&now,NULL);
    long timeuse = 1000000 * (now.tv_sec - pkt.timestamp.tv_sec) + now.tv_usec
              - pkt.timestamp.tv_usec;

    if(timeuse > 500000){
        if(pkt.isKeyFrame){
            result = 1;
        }else{
            result = -1;
            dataQueue.pop();
        }
        droppingMode = true;
    }else{
        droppingMode = false;
    }
    pthread_mutex_unlock(&queueLock);
    return result;
}

void *decodeThread(void __unused *arg){
    isDecodeThreadRunning = true;
    while(isDecodeThreadRunning){
        if(!dataQueue.empty()){
            if(droppingMode){
                bool skip = false;
                pthread_mutex_lock(&queueLock);
                if(!dataQueue.front().isKeyFrame){
                    dataQueue.pop();
                    skip = true;
                }
                pthread_mutex_unlock(&queueLock);
                if(skip) {
                    continue;
                }
            }

            int result = shouldDropFrame();
            if(result != 0){
                if(mCallback != NULL){
                    mCallback->frameDropped();
                }
                if(result == -1){
                    continue;
                }
            }

            ssize_t index = AMediaCodec_dequeueInputBuffer(mediaCodec, 0);
            if(index >= 0){
                size_t bs;//buffer size;
                uint8_t *buf = AMediaCodec_getInputBuffer(mediaCodec, static_cast<size_t>(index), &bs);
                if(buf == NULL) continue;
                memset(buf,0,bs);

                pthread_mutex_lock(&queueLock);
                VideoPacket pkt = dataQueue.front();
                size_t fill_len = pkt.len;
                if(pkt.len > bs){
                    LOGE("input buffer length < packet len %d:%d",pkt.len,bs);
                    fill_len = bs;
                }
                memcpy(buf,pkt.buffer,fill_len);
                dataQueue.pop();
                pthread_mutex_unlock(&queueLock);
                AMediaCodec_queueInputBuffer(mediaCodec, static_cast<size_t>(index), 0, fill_len,
                                             static_cast<uint64_t>(numPkts * 30), 0);

                numPkts++;
            }//index >= 0 getInputBuffer
            AMediaCodecBufferInfo bufferInfo = {0};
            index = AMediaCodec_dequeueOutputBuffer(mediaCodec, &bufferInfo, 0);

            if(index < 0){
                if(index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED){
                    //TODO do something
                    LOGE("output format changed");
                    AMediaFormat *newFormat = AMediaCodec_getOutputFormat(mediaCodec);
                    bool result = AMediaFormat_getInt32(newFormat,AMEDIAFORMAT_KEY_COLOR_FORMAT,&output_colorFormat);
                    LOGE("color format %d,%d",result,output_colorFormat);
                }
            }
            while (index >= 0){
                size_t out_bs;//buffer size;
                uint8_t *outBuffer = AMediaCodec_getOutputBuffer(mediaCodec, static_cast<size_t>(index), &out_bs);
                if(outBuffer == NULL){
                    break;
                }
                if(_sender != NULL){
                    FramePacket pkt = FramePacket(outBuffer,out_bs,width,height,output_colorFormat,notify_nv21,draw_by_user);
                    _sender->enqueueFrame(pkt);
                }
                if(mCallback != NULL){
                    mCallback->frameDecoded();
                }

                AMediaCodec_releaseOutputBuffer(mediaCodec, static_cast<size_t>(index), false);
                index = AMediaCodec_dequeueOutputBuffer(mediaCodec, &bufferInfo, 0);
            }//while(index >= 0)
        }//if(!dataQueue.empty())
    }
    LOGI("decode thread quit");
    return NULL;
}

void mediaCodec_enqueuePacket(unsigned char *packet,size_t len,bool keyframe){
    if(mediaCodec == NULL){
        return;
    }

    if(droppingMode){
        if(!keyframe) return;
    }

    VideoPacket pkt = VideoPacket(packet,len,keyframe);
    pthread_mutex_lock(&queueLock);
    dataQueue.push(pkt);
    pthread_mutex_unlock(&queueLock);
}
void mediaCodec_onSPSFrame(unsigned char *buffer, size_t len,int w,int h){
    if(spsBuffer != NULL){
        free(spsBuffer);
        spsBuffer = NULL;
        spsBuffer_size = 0;
    }

    width = w;
    height = h;

    spsBuffer = (unsigned char *)malloc(len);
    if(spsBuffer == NULL) {
        LOGE("malloc sps buffer failed");
        return;
    }
    memcpy(spsBuffer,buffer,len);
    spsBuffer_size = len;

    if(_sender != NULL){
        _sender->sizeChanged(width,height);
    }
}

void mediaCodec_onPPSFrame(unsigned char *buffer,size_t len){
    if(ppsBuffer != NULL){
        free(ppsBuffer);
        ppsBuffer = NULL;
        ppsBuffer_size = 0;
    }

    ppsBuffer = (unsigned char *)malloc(len);
    if(ppsBuffer == NULL) {
        LOGE("malloc pps buffer failed");
        return;
    }
    memcpy(ppsBuffer,buffer,len);
    ppsBuffer_size = len;
}

void mediaCodec_onFrame(unsigned char *buffer,int len,bool keyframe){
    if(mediaCodec == NULL){
        LOGI("create media codec");
        media_status_t result;
        mediaCodec = AMediaCodec_createDecoderByType(MIME_TYPE);
        if(mediaCodec == NULL){
            LOGE("create codec failed");
            return;
        }

        AMediaFormat *format = AMediaFormat_new();
        if(format == NULL){
            AMediaCodec_delete(mediaCodec);
            mediaCodec = NULL;
            LOGE("cannot create media format");
            return;
        }
        AMediaFormat_setString(format,AMEDIAFORMAT_KEY_MIME,MIME_TYPE);
        AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_WIDTH,width);
        AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_HEIGHT,height);
//        AMediaFormat_setInt32(format,AMEDIAFORMAT_KEY_COLOR_FORMAT,0x7f420888);

        unsigned char *data = spsBuffer == NULL ? SPS : spsBuffer;
        size_t data_size = spsBuffer == NULL ? sizeof(SPS) : spsBuffer_size;
        AMediaFormat_setBuffer(format,CSD0,data,data_size);

        data = ppsBuffer == NULL ? PPS : ppsBuffer;
        data_size = ppsBuffer == NULL ? sizeof(PPS) :ppsBuffer_size;
        AMediaFormat_setBuffer(format,CSD1,data,data_size);

        result = AMediaCodec_configure(mediaCodec,format,NULL,NULL,0);
        if(result < 0){
            AMediaFormat_delete(format);
            AMediaCodec_delete(mediaCodec);
            mediaCodec = NULL;
            LOGE("configure codec failed %d",result);
            return;
        }
        result = AMediaCodec_start(mediaCodec);
        if(result < 0){
            AMediaFormat_delete(format);
            AMediaCodec_delete(mediaCodec);
            mediaCodec = NULL;
            LOGE("start codec failed %d",result);
            return;
        }

        AMediaFormat_delete(format);
        pthread_create(&decode_tid,NULL,decodeThread,NULL);
        LOGI("create decode thread,%ld",decode_tid);
    }
    mediaCodec_enqueuePacket(buffer, static_cast<size_t>(len),keyframe);
}

void mediaCodec_setSurface(ANativeWindow *surface){
//    if(videoSurface){
//        ANativeWindow_release(videoSurface);
//    }
    videoSurface = surface;
}

void mediaCodec_create(mediaCodecCallback *callback){
    pthread_mutex_init(&queueLock,NULL);
    mCallback = callback;

    if(_sender != NULL){
        delete(_sender);
        _sender = NULL;
    }
    _sender = new senderThread(mCallback);
}

void mediaCodec_destroy(){
    isDecodeThreadRunning = false;
    LOGI("decodee tid %ld",decode_tid);
    if(decode_tid != -1){
        pthread_join(decode_tid,NULL);
        LOGI("waiting decode thread ok");
        decode_tid = -1;
    }

    if(_sender != NULL){
        delete(_sender);
        _sender = NULL;
    }

    if(spsBuffer != NULL){
        free(spsBuffer);
        spsBuffer = NULL;
        spsBuffer_size = 0;
    }
    if(ppsBuffer != NULL){
        free(ppsBuffer);
        ppsBuffer = NULL;
        ppsBuffer_size = 0;
    }
    if(mediaCodec != NULL){
        media_status_t result;
        result = AMediaCodec_stop(mediaCodec);
        LOGI("stop mediacodec result :%d",result);
        result = AMediaCodec_delete(mediaCodec);
        LOGI("free mediacodec result :%d",result);
        mediaCodec = NULL;
    }
//    if(videoSurface != NULL){
//        ANativeWindow_release(videoSurface);
//        videoSurface = NULL;
//    }
    clearQueue(&dataQueue);
    pthread_mutex_destroy(&queueLock);
    numPkts = 0;
}

void mediaCodec_drawByUser(bool enable){
    draw_by_user = enable;
}
void mediaCodec_notifyNV21(bool enable){
    notify_nv21 = enable;
}
