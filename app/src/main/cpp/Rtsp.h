#ifndef ANDROID_SVN_2_RTSP_H
#define ANDROID_SVN_2_RTSP_H

#include <pthread.h>
#include "BasicUsageEnvironment.hh"
#include "ServerMediaSession.hh"
#include "liveMedia.hh"

typedef void (*onRTSPFrameAvailable)(uint8_t *buffer, unsigned len);

class StreamClientState {
public:
    StreamClientState();

    virtual ~StreamClientState();
public:
    MediaSubsessionIterator* iter;
    MediaSession* session;
    MediaSubsession* subsession;
    TaskToken streamTimerTask;
    double duration;
};

class MyRtspClient: public RTSPClient {
public:
    static MyRtspClient* createNew(UsageEnvironment& env, char const* rtspURL,
                                    int verbosityLevel = 0, char const* applicationName = NULL,
                                    portNumBits tunnelOverHTTPPortNum = 0);

    MyRtspClient(UsageEnvironment& env, char const* rtspURL,
                  int verbosityLevel, char const* applicationName,
                  portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
    virtual ~MyRtspClient();

public:
    StreamClientState    scs;
    onRTSPFrameAvailable frameCallback;
    bool isPlaying;
};

class Rtsp{

public:
    Rtsp();

    int  start(char const* rtspURL);
    void setFrameCallback(onRTSPFrameAvailable frameAvailable);
    void shutDown();
    bool isPlaying();
    ~Rtsp();

private:
    UsageEnvironment* env;
    MyRtspClient *rtsp_client;

    int  isShutDown;

    static void shutdownStream(RTSPClient* rtspClient);

    pthread_t envThead_pt;
    static void *envThead(void* arg);
    char eventLoopWatchVariable;

    static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode,char* resultString);
    static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode,char* resultString);
    static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode,char* resultString);
    static void setupNextSubsession(RTSPClient* rtspClient);

    static void subsessionAfterPlaying(void* clientData);
    static void subsessionByeHandler(void* clientData);
    static void streamTimerHandler(void* clientData);
};
#endif //ANDROID_SVN_2_RTSP_H
