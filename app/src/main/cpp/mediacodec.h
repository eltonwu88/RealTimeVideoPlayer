#include <cstdlib>
#include <android/native_window_jni.h>

#ifndef UDPLIVE_MEDIACODEC_H
#define UDPLIVE_MEDIACODEC_H

struct mediaCodecCallback{
    void (*nv21Callback)(uint8_t *buffer,size_t len,int width,int height);
    void (*argbCallback)(uint8_t *buffer,size_t len,int width,int height);
    void (*frameDecoded)(void);
    void (*frameDropped)(void);
    void (*frameRendered)(void);
};
typedef struct mediaCodecCallback mediaCodecCallback;

void mediaCodec_onSPSFrame(unsigned char *buffer, size_t len,int w,int h);
void mediaCodec_onPPSFrame(unsigned char *buffer,size_t len);
void mediaCodec_onFrame(unsigned char *buffer,int len,bool keyframe);
void mediaCodec_setSurface(ANativeWindow *surface);
void mediaCodec_create(mediaCodecCallback *callback);
void mediaCodec_destroy();
void mediaCodec_drawByUser(bool enable);
void mediaCodec_notifyNV21(bool enable);

#endif //UDPLIVE_MEDIACODEC_H
