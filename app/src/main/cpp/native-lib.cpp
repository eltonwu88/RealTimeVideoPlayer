#include <climits>
#include <jni.h>
#include <string>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <android/native_window_jni.h>
#include <cmath>
#include "log.h"
#include "h264_stream.h"
#include "mediacodec.h"

#define UDP_LIVE_PORT       2234
#define TCP_CMD_PORT        4646

jobject nativeInstance = NULL;
JavaVM *JVM;
jmethodID onFrameMethodID;

pthread_t work_threadID = -1;
volatile bool isWorkRunning;

int udpFD=0;
//decode var
int lastPFrameNum=-2;
bool isLostFrame=false;

static mediaCodecCallback mCallback;
static volatile long  byteReceived;
static long  byteRate;
static volatile int   frameReceived;
static volatile int   frameDecoded;
static volatile int   frameDropped;
static volatile int   frameRendered;
static float fps;

void signalHandler(int signo)
{
    switch (signo){
        case SIGALRM:
            byteRate = byteReceived;
            fps      = frameDecoded;
            LOGD("fps :%.1f,BR :%ld,Fr %d,Dropped %d,Rendered %d",fps,byteRate,frameReceived,frameDropped,frameRendered);
            byteReceived = frameDecoded = frameReceived = frameDropped = frameRendered = 0;
            break;
        default:
            break;
    }
}

void Android_OnFrame(u_int8_t* buffer, unsigned frameSize, int width, int height,int format) {
    JNIEnv *mEnv;
    JVM->AttachCurrentThread(&mEnv, NULL);
    if (onFrameMethodID) {
        /*alloc java array */
        const char *array_name;
        if(format == 1){
            array_name = "nv21array";
        }else {
            array_name = "argbarray";
        }
        jclass   cls = mEnv->GetObjectClass(nativeInstance);
        jfieldID fid = mEnv->GetFieldID(cls,array_name,"[B");
        jobject  obj = mEnv->GetObjectField(nativeInstance,fid);
        if(obj == NULL){
            jbyteArray jbarray = mEnv->NewByteArray(frameSize);
            mEnv->SetByteArrayRegion(jbarray, 0, frameSize, (const jbyte *)buffer);
            mEnv->SetObjectField(nativeInstance,fid,jbarray);
            mEnv->DeleteLocalRef(jbarray);
            LOGE("new array %s",array_name);
        }else {
            jsize len = mEnv->GetArrayLength(static_cast<jarray>(obj));
            if(len != frameSize){
                jbyteArray jbarray = mEnv->NewByteArray(frameSize);
                mEnv->SetByteArrayRegion(jbarray, 0, frameSize, (const jbyte *)buffer);
                mEnv->SetObjectField(nativeInstance,fid,jbarray);
                mEnv->DeleteLocalRef(jbarray);
                LOGE("array changed %s",array_name);
            }else{
                jbyteArray jbarray = static_cast<jbyteArray>(obj);
                mEnv->SetByteArrayRegion(jbarray, 0, frameSize, (const jbyte *)buffer);
            }
            mEnv->DeleteLocalRef(obj);
        }
        /* end of alloc array*/
        jbyteArray jbarray = static_cast<jbyteArray>(mEnv->GetObjectField(nativeInstance, fid));
        mEnv->CallVoidMethod(nativeInstance, onFrameMethodID, format,jbarray, width, height);
        mEnv->DeleteLocalRef(jbarray);
        mEnv->DeleteLocalRef(cls);
    }
    JVM->DetachCurrentThread();
}

void onNV21Callback(uint8_t *buffer,size_t len,int width,int height){
    Android_OnFrame(buffer,len,width,height,1);
}
void onARGBCallback(uint8_t *buffer,size_t len,int width,int height){
    Android_OnFrame(buffer,len,width,height,2);
}
void onFrameDecoded(void){
    frameDecoded ++;
}
void onFrameDropped(void){
    frameDropped ++;
}
void onFrameRendered(void){
    frameRendered++;
}

void decodeFrame(u_int8_t * inBuffer,unsigned inSize)
{
    static int frame_num_max;
    h264_stream_t* h= h264_new();

    unsigned frameSize=	inSize-4;
    u_int8_t* fReceiveBuffer=inBuffer+4;
    read_nal_unit(h, fReceiveBuffer, frameSize);
//    LOGD("h264 nal_unit_type: %d ", h->nal->nal_unit_type);

    if(h->nal->nal_unit_type == 7) {
        int width = (h->sps->pic_width_in_mbs_minus1 + 1) * 16;
        int height = (h->sps->pic_height_in_map_units_minus1 + 1) * 16;
        frame_num_max = (int)pow(2, (h->sps->log2_max_frame_num_minus4+4));
        LOGI("h264 get sps frame, width: %d, height: %d, frame_num_max: %d", width, height, frame_num_max);
//        Android_JNI_OnSPSFrame(fReceiveBuffer, frameSize, width, height);
        mediaCodec_onSPSFrame(inBuffer,inSize,width,height);
    }
    else if(h->nal->nal_unit_type == 8) {
        LOGD("h264 get pps frame.");
//        Android_JNI_OnPPSFrame(fReceiveBuffer, frameSize);
        mediaCodec_onPPSFrame(inBuffer,inSize);

    }
    else if(h->nal->nal_unit_type == 1) {
#if 1
        if(isLostFrame) {
//            LOGD("h264 error p frame, lost this. frame_num: %d, lastPFrameNum: %d", h->sh->frame_num, lastPFrameNum);
        }
        else if(h->sh->frame_num!=lastPFrameNum+1) {
            isLostFrame = true;
//            LOGD("h264 error p frame, lost this. frame_num: %d, lastPFrameNum: %d", h->sh->frame_num, lastPFrameNum);
        }
#endif
        else {
//            LOGD("h264 correct p frame.");
//            Android_JNI_OnFrame(fReceiveBuffer, frameSize);
            mediaCodec_onFrame(inBuffer,inSize,false);

            lastPFrameNum = h->sh->frame_num;
            if(lastPFrameNum==(frame_num_max-1)) {
                lastPFrameNum = -1;
            }
        }
    }
    else if(h->nal->nal_unit_type == 5){
//        Android_JNI_OnFrame(fReceiveBuffer, frameSize);
        mediaCodec_onFrame(inBuffer,inSize,true);
        LOGD("h->sh->frame_nume %d ", h->sh->frame_num);
        lastPFrameNum = h->sh->frame_num;
        isLostFrame = false;
    }
    else {
//        Android_JNI_OnFrame(fReceiveBuffer, frameSize);
        mediaCodec_onFrame(inBuffer,inSize, false);
    }
    h264_free(h);
}

int oneShotTcpCMD(const char *cmd){
    int tcpFD = socket(AF_INET, SOCK_STREAM, 0);
    if(tcpFD < 0){
        LOGE("create udp fails %s,%d",strerror(errno),errno);
        return -1;
    }
    struct sockaddr_in tcp_server;
    bzero(&tcp_server, sizeof(tcp_server));
    tcp_server.sin_family = AF_INET;
    tcp_server.sin_port = htons(TCP_CMD_PORT);
    tcp_server.sin_addr.s_addr = inet_addr("192.168.1.254");

    struct timeval time;
    time.tv_sec = 3;
    time.tv_usec= 0;
    if(setsockopt(tcpFD,SOL_SOCKET,SO_RCVTIMEO,&time,sizeof(time))<0){
        LOGE("setsockopt recv timeout fail %s",strerror(errno));
        close(tcpFD);
        return -1;
    }
    if(setsockopt(tcpFD,SOL_SOCKET,SO_SNDTIMEO,&time,sizeof(time))<0){
        LOGE("setsockopt send timeout fail %s",strerror(errno));
        close(tcpFD);
        return -1;
    }

    if(connect(tcpFD,(struct sockaddr*)&tcp_server,sizeof(tcp_server)) < 0){
        LOGE("## connect tcp fail [%d]: %s", errno, strerror(errno));
        close(tcpFD);
        return -1;
    }
    if(send(tcpFD,cmd,strlen(cmd),0)<0){
        LOGE("send tcp cmd error %s,%d",strerror(errno),errno);
    }
    LOGI("receiving rsp...");
    unsigned char buffer[100];
    memset(buffer,sizeof(buffer),0);
    int len = (int)recv(tcpFD,buffer,sizeof(buffer),0);
    if(len > 0){
        LOGI("recv rsp :%s",buffer);
    }else{
        LOGE("recv error: %s,%d",strerror(errno),errno);
    }
    close(tcpFD);
    return 0;
}

void *workThread(void *){
    struct sockaddr_in udp_server;
    bzero(&udp_server, sizeof(udp_server));
    udp_server.sin_family = AF_INET;
    udp_server.sin_port = htons(UDP_LIVE_PORT);
    udp_server.sin_addr.s_addr = inet_addr("192.168.1.254");

    unsigned char message[10];
    unsigned char recvBuffer[100000];
//    char ack[10];


    oneShotTcpCMD("{\"CMD\":2,\"PARAM\":-1}");
    usleep(200000);//200ms
    memset(message,0x55,sizeof(message));
    LOGD("send udp aloha");
    if(sendto(udpFD,message,sizeof(message),0,(struct sockaddr*)&udp_server,sizeof(struct sockaddr_in))<0){
        LOGE("udp send failed %s,%d",strerror(errno),errno);
        return NULL;
    }

    isWorkRunning = true;
    struct sockaddr_in incoming = {0};
    size_t incoming_len = sizeof(incoming);
    bzero(&incoming,sizeof(incoming));
    LOGI("start working...");
    struct timeval start,end;
    while(isWorkRunning){
        gettimeofday(&start,NULL);
        int recvLength = (unsigned int)recvfrom(udpFD,recvBuffer,sizeof(recvBuffer),0,(sockaddr *) &incoming,(socklen_t *) &incoming_len);
        gettimeofday(&end,NULL);
        long timeuse = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec
                       - start.tv_usec;
        LOGI("recv from using %ld",timeuse);
        if(recvLength <= 0){
            if(errno != EAGAIN){
                LOGI("recv %s,%d",strerror(errno),errno);
                break;
            }
            continue;
        }
//        sprintf(ack,"%d",recvLength);
//        if(sendto(udpFD,ack,sizeof(ack),0,(struct sockaddr*)&udp_server,sizeof(struct sockaddr_in)) < 0){
//            LOGI("send ack %s,%d",strerror(errno),errno);
//        }
        byteReceived += recvLength;
        frameReceived++;

        decodeFrame(recvBuffer,(unsigned)recvLength);
    }
    oneShotTcpCMD("{\"CMD\":3,\"PARAM\":-1}");
    LOGI("working thread quit");
    return NULL;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_UDPLive_init(JNIEnv *env, jobject instance) {
    nativeInstance = env->NewGlobalRef(instance);
    if(nativeInstance == NULL){
        LOGI("failed to global ref instance");
    }
    LOGE("init");
    env->GetJavaVM(&JVM);
    if ((udpFD = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOGE("create udp fails %s,%d",strerror(errno),errno);
        return;
    }

    jclass cls = env->GetObjectClass(instance);
    onFrameMethodID= env->GetMethodID(cls, "onFrame", "(I[BII)V");

    mCallback.argbCallback = onARGBCallback;
    mCallback.nv21Callback = onNV21Callback;
    mCallback.frameDecoded = onFrameDecoded;
    mCallback.frameDropped = onFrameDropped;
    mCallback.frameRendered= onFrameRendered;
    mediaCodec_create(&mCallback);
    mediaCodec_drawByUser(true);

//    mediaCodec_notifyNV21(true);
    pthread_create(&work_threadID,NULL,workThread,NULL);

    signal(SIGALRM,signalHandler);
    struct itimerval new_value, old_value;
    new_value.it_value.tv_sec = 1;
    new_value.it_value.tv_usec = 0;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_value, &old_value);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_UDPLive_deinit(JNIEnv *env, jobject __unused instance) {
    LOGE("deinit");
    struct itimerval new_value, old_value;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_usec = 0;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_value, &old_value);

    if(udpFD > 0){
        shutdown(udpFD,SHUT_RDWR);
    }
    isWorkRunning = false;
    if(work_threadID != -1){
        pthread_join(work_threadID,NULL);
        work_threadID = -1;
        LOGI("waiting work thread ok");
    }
    mediaCodec_destroy();
    if(udpFD > 0){
        close(udpFD);
        udpFD = -1;
    }
    if(nativeInstance != NULL){
        env->DeleteGlobalRef(nativeInstance);
        nativeInstance = NULL;
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_UDPLive_setSurface(JNIEnv *env, jobject  __unused instance,
                                                    jobject surface) {
    if(surface){
        ANativeWindow *window = ANativeWindow_fromSurface(env,surface);
        if (window){
            mediaCodec_setSurface(window);
            ANativeWindow_release(window);
        }else{
            LOGE("failed to get native widnow from surface");
        }
    }else{
        LOGE("surface still null");
    }
}