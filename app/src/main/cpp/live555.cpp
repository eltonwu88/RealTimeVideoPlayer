#include <jni.h>
#include <unistd.h>
#include <android/native_window_jni.h>
#include <cmath>
#include "log.h"
#include "h264_stream.h"
#include "mediacodec.h"
#include "Rtsp.h"

static jobject nativeInstance = NULL;
static JavaVM *JVM;
static jmethodID onFrameMethodID;
static int lastPFrameNum=-2;
static bool isLostFrame=false;

static mediaCodecCallback mCallback;
static volatile long  byteReceived;
static long  byteRate;
static volatile int   frameReceived;
static volatile int   frameDecoded;
static volatile int   frameDropped;
static volatile int   frameRendered;
static float fps;

static Rtsp *rtsp = NULL;

static void signalHandler(int signo)
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

static void Android_OnFrame(u_int8_t* buffer, unsigned frameSize, int width, int height,int format) {
    if(buffer == NULL){
        LOGE("exception !!! frame buffer is null");
        return;

    }

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

static void onNV21Callback(uint8_t *buffer,size_t len,int width,int height){
    Android_OnFrame(buffer,len,width,height,1);
}
static void onARGBCallback(uint8_t *buffer,size_t len,int width,int height){
    Android_OnFrame(buffer,len,width,height,2);
}
static void onFrameDecoded(void){
    frameDecoded ++;
}
static void onFrameDropped(void){
    frameDropped ++;
}
static void onFrameRendered(void){
    frameRendered++;
}

static void decodeFrame(u_int8_t * inBuffer,unsigned inSize)
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
//        LOGI("frame num %d, last %d,max %d",h->sh->frame_num,lastPFrameNum,frame_num_max);
        if(isLostFrame) {
//            LOGD("h264 error p frame, lost this. frame_num: %d, lastPFrameNum: %d", h->sh->frame_num, lastPFrameNum);
        }
        else if(h->sh->frame_num < lastPFrameNum) {
            isLostFrame = true;
//            LOGD("h264 error p frame, lost this. frame_num: %d, lastPFrameNum: %d", h->sh->frame_num, lastPFrameNum);
        }
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

static void onFrameAvailable(uint8_t *buffer, unsigned len){
    uint8_t *normalized = (uint8_t *)(malloc(len + 4));
    if(normalized == NULL) {
        LOGE("failed to malloc normalizedd buffer");
        return;
    }
    normalized[0] = 0;
    normalized[1] = 0;
    normalized[2] = 0;
    normalized[3] = 1;
    uint8_t *ptr = normalized + 4;
    memcpy(ptr,buffer,len);
//    LOGE("got frame %d,%d,%d,%d,%d len : %d",normalized[0],normalized[1],normalized[2],normalized[3],normalized[4],len);
    decodeFrame(normalized,len+4);
    byteReceived++;
    free(normalized);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_Live555_deinit(JNIEnv __unused *env, jobject __unused instance) {
    LOGE("deinit");
    struct itimerval new_value, old_value;
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_usec = 0;
    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_value, &old_value);

    if(rtsp != NULL){
        rtsp->shutDown();
        rtsp = NULL;
    }

    mediaCodec_destroy();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_Live555_setSurface(JNIEnv *env, jobject __unused instance, jobject surface) {
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

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_Live555_nv21notify(JNIEnv __unused *env, jobject __unused instance, jboolean enable) {
    mediaCodec_notifyNV21(enable == JNI_TRUE);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_Live555_drawByUser(JNIEnv __unused *env, jobject __unused instance, jboolean enable) {
    mediaCodec_drawByUser(enable == JNI_TRUE);
}

extern "C"
JNIEXPORT jboolean JNICALL
        Java_com_example_eltonwu_udplive_Live555_isPlaying(JNIEnv __unused *env, jobject __unused instance) {
    if(rtsp == NULL){
        return JNI_FALSE;
    }else{
        if(rtsp->isPlaying()){
            return JNI_TRUE;
        }else{
            return JNI_FALSE;
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_eltonwu_udplive_Live555_init(JNIEnv *env, jobject instance, jstring url_) {
    const char *url = env->GetStringUTFChars(url_, 0);

    nativeInstance = env->NewGlobalRef(instance);
    if(nativeInstance == NULL){
        LOGI("failed to global ref instance");
    }
    LOGE("init");
    env->GetJavaVM(&JVM);

    jclass cls = env->GetObjectClass(instance);
    onFrameMethodID= env->GetMethodID(cls, "onFrame", "(I[BII)V");

    mCallback.argbCallback = onARGBCallback;
    mCallback.nv21Callback = onNV21Callback;
    mCallback.frameDecoded = onFrameDecoded;
    mCallback.frameDropped = onFrameDropped;
    mCallback.frameRendered= onFrameRendered;
    mediaCodec_create(&mCallback);
    mediaCodec_drawByUser(true);

    signal(SIGALRM,signalHandler);
    struct itimerval new_value, old_value;
    new_value.it_value.tv_sec = 1;
    new_value.it_value.tv_usec = 0;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &new_value, &old_value);

    int result;
    rtsp = new Rtsp();
    if(rtsp == NULL){
        LOGE("failed to create rtsp");
        goto func_end;
    }
    result = rtsp->start(url);
    LOGI("rtsp start result %d",result);
    rtsp->setFrameCallback(onFrameAvailable);
func_end:
    env->ReleaseStringUTFChars(url_, url);
}