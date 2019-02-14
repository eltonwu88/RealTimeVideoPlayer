//
// Created by eltonwu on 2018/11/22.
//

#ifndef UDPLIVE_SENDERTHREAD_H
#define UDPLIVE_SENDERTHREAD_H

#include <pthread.h>
#include <queue>
#include "FramePacket.h"
#include "mediacodec.h"

class senderThread {
public:
    senderThread(mediaCodecCallback *cb);
    ~senderThread();

    void enqueueFrame(FramePacket &p);
    void sizeChanged(int width,int height);

private:
    pthread_mutex_t queueLock;
    pthread_t tid;
    std::queue<FramePacket> frameQueue;
    bool isRunning;

    uint8_t *argb_array;
    size_t  argb_size;
    uint8_t *nv21_array;
    size_t  nv21_size;

    static void *workThread(void *arg);
    int toARGB(FramePacket &p);
    int toNV21(FramePacket &p);

    /*yuv type 0 :nv21,1:nv12,2:I420*/
    void getStride(int width,int height,int colorFormat,/*input*/
                   int &yStride,int &uvStride,int &ySpan,int &uvSpan,int &yuvType);/*out*/
    mediaCodecCallback *callback;
};


#endif //UDPLIVE_SENDERTHREAD_H
