#include <sys/time.h>
#include "NetAddress.hh"

#include "Rtsp.h"
#include "log.h"

void *Rtsp::envThead(void* arg) {
    Rtsp *rtsp = static_cast<Rtsp *>(arg);
    LOGI("envThead:%d", rtsp->eventLoopWatchVariable);
    rtsp->env->taskScheduler().doEventLoop(&rtsp->eventLoopWatchVariable);
    LOGE("cleanUp null?");
    if (rtsp->eventLoopWatchVariable != 0) {
        rtsp->eventLoopWatchVariable = 0;
        if (rtsp->rtsp_client) {
            rtsp->shutdownStream(rtsp->rtsp_client);
            rtsp->rtsp_client = NULL;
        }
        rtsp->isShutDown = 0;
    }
    LOGE("envThead end=%p", rtsp->rtsp_client);
    pthread_exit(0);
}

StreamClientState::StreamClientState() :
        iter(NULL),
        session(NULL),
        subsession(NULL),
        streamTimerTask(NULL),
        duration(
        0.0){
    //empty
}

StreamClientState::~StreamClientState() {
    delete iter;
    if (session != NULL) {
        // We also need to delete "session", and unschedule "streamTimerTask" (if set)
        UsageEnvironment &env = session->envir(); // alias

        env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
        Medium::close(session);
    }
}

class VideoSink: public MediaSink {
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 100000
public:
    static VideoSink* createNew(UsageEnvironment& env,
                                MediaSubsession& subsession, // identifies the kind of data that's being received
                                char const* streamId = NULL,// identifies the stream itself (optional)
                                onRTSPFrameAvailable callback = NULL) {
        return new VideoSink(env, subsession, streamId,callback);
    }

private:
    onRTSPFrameAvailable callback;
private:
    // called only by "createNew()"
    VideoSink(UsageEnvironment& env, MediaSubsession& subsession,
              char const* streamId,onRTSPFrameAvailable callback) :
            MediaSink(env), fSubsession(subsession),callback(callback) {
        fStreamId = strDup(streamId);
        fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
    }

    virtual ~VideoSink(){
        delete[] fReceiveBuffer;
        delete[] fStreamId;
    }

    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes, struct timeval presentationTime,
                                  unsigned durationInMicroseconds){
        VideoSink* sink = (VideoSink*) clientData;
        sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime,
                                durationInMicroseconds);
    }

    void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                           struct timeval presentationTime, unsigned durationInMicroseconds){
        if (!strcmp(fSubsession.mediumName(), "video")) {
            if(callback != NULL){
                callback(fReceiveBuffer,frameSize);
            }
        }
        continuePlaying();
    }
    // redefined virtual functions:
    virtual Boolean continuePlaying(){
        if (fSource == NULL) {
            LOGD("fSource == NULL");
            return False; // sanity check (should not happen)
        }
        // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
        fSource->getNextFrame(fReceiveBuffer, DUMMY_SINK_RECEIVE_BUFFER_SIZE,
                              afterGettingFrame, this, onSourceClosure, this);
        return True;
    }

private:
    u_int8_t* fReceiveBuffer;
    MediaSubsession& fSubsession;
    char* fStreamId;
};

MyRtspClient* MyRtspClient::createNew(UsageEnvironment& env,
                                        char const* rtspURL, int verbosityLevel, char const* applicationName,
                                        portNumBits tunnelOverHTTPPortNum) {
    return new MyRtspClient(env, rtspURL, verbosityLevel, applicationName,
                             tunnelOverHTTPPortNum);
}

MyRtspClient::MyRtspClient(UsageEnvironment& env, char const* rtspURL,
                             int verbosityLevel, char const* applicationName,
                             portNumBits tunnelOverHTTPPortNum) :
        RTSPClient(env, rtspURL, verbosityLevel, applicationName,
                   tunnelOverHTTPPortNum, -1) {
    isPlaying = False;
}

MyRtspClient::~MyRtspClient() {
}

Rtsp::Rtsp(){
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    isShutDown = 0;
    eventLoopWatchVariable = 0;
    rtsp_client = NULL;
}

Rtsp::~Rtsp() {
    if(rtsp_client != NULL){
        delete rtsp_client;
        rtsp_client = NULL;
    }

}

void Rtsp::setFrameCallback(onRTSPFrameAvailable callback) {
    if(rtsp_client != NULL){
        rtsp_client->frameCallback = callback;
    }
}

int Rtsp::start(char const* rtspURL){
    if(rtsp_client){
        LOGE("rtsp already existed");
    }
    rtsp_client = MyRtspClient::createNew(*env, rtspURL,1,"elanview");
    if (rtsp_client == NULL) {
        LOGE("Failed to create a RTSP client for URL \":%s,%s", rtspURL, env->getResultMsg());
        return -1;
    }
    unsigned cmd = rtsp_client->sendDescribeCommand(continueAfterDESCRIBE);
    LOGD("rtsp cmd:%u", cmd);

    pthread_create(&envThead_pt, NULL, &envThead, this);
    return 0;
}

void Rtsp::shutDown() {
    isShutDown = 1;
    eventLoopWatchVariable = '1';

    while (isShutDown) {
        usleep(100);
    }
}

void Rtsp::shutdownStream(RTSPClient* rtspClient) {
    LOGI("shutdownStream");
    if (rtspClient) {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((MyRtspClient*) rtspClient)->scs; // alias

        // First, check whether any subsessions have still to be closed:
        if (scs.session != NULL) {
            Boolean someSubsessionsWereActive = False;
            MediaSubsessionIterator iter(*scs.session);
            MediaSubsession* subsession;

            while ((subsession = iter.next()) != NULL) {
                if (subsession->sink != NULL) {
                    Medium::close(subsession->sink);
                    subsession->sink = NULL;

                    if (subsession->rtcpInstance() != NULL) {
                        subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
                    }
                    someSubsessionsWereActive = True;
                }
            }
            if (someSubsessionsWereActive) {
                // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
                // Don't bother handling the response to the "TEARDOWN".
                rtspClient->sendTeardownCommand(*scs.session, NULL);
            }
        }
        LOGI("Closing the stream.\n");
        //env << *rtspClient << "Closing the stream.\n";
        Medium::close(rtspClient);
        LOGI("Medium::close(rtspClient)");
    }
    LOGI("shutdownStream end.\n");
    ((MyRtspClient*) rtspClient)->isPlaying = false;
}

void Rtsp::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode,
                           char* resultString) {
    LOGI("continueAfterDESCRIBE");
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((MyRtspClient*) rtspClient)->scs; // alias

        if (resultCode != 0) {
            LOGE("Failed to get a SDP description: %s", resultString);
            delete[] resultString;
            break;
        }

        char* const sdpDescription = resultString;
        LOGI("Got a SDP description:\n %s", sdpDescription);
        // Create a media session object from this SDP description:
        scs.session = MediaSession::createNew(env, sdpDescription);
        delete[] sdpDescription; // because we don't need it anymore
        if (scs.session == NULL) {
            LOGI("Failed to create a MediaSession object from the SDP description: %s", env.getResultMsg());
            break;
        } else if (!scs.session->hasSubsessions()) {
            break;
        }
        scs.iter = new MediaSubsessionIterator(*scs.session);
        setupNextSubsession(rtspClient);
        return;
    } while (0);
    LOGE("error after DESCRIBE");
    // An unrecoverable error occurred with this stream.
}

#define REQUEST_STREAMING_OVER_TCP True
void Rtsp::setupNextSubsession(RTSPClient* rtspClient){
    LOGI("setupNextSubsession");
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((MyRtspClient*) rtspClient)->scs; // alias

    scs.subsession = scs.iter->next();
    if (scs.subsession != NULL) {
        if (!scs.subsession->initiate()) {
            setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
        } else {
            if (scs.subsession->rtcpIsMuxed()) {
                LOGI("client port %d ", scs.subsession->clientPortNum());
            } else {
                LOGI("client ports ");
            }
            rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP,
                                         False, REQUEST_STREAMING_OVER_TCP);
        }
        return;
    }
    if (scs.session->absStartTime() != NULL) {
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY,
                                    scs.session->absStartTime(), scs.session->absEndTime());
    } else {
        scs.duration = scs.session->playEndTime()
                       - scs.session->playStartTime();
        rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
    }
}

void Rtsp::continueAfterSETUP(RTSPClient* rtspClient, int resultCode,
                        char* resultString) {
    LOGI("continueAfterSETUP");
    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((MyRtspClient*) rtspClient)->scs; // alias

        if (resultCode != 0) {
            LOGI("continueAfterSETUP:Failed to set up the \"");
            break;
        }
        if (scs.subsession->rtcpIsMuxed()) {
            LOGI(
                    "continueAfterSETUP:client port:%c", char(scs.subsession->clientPortNum()));
        } else {

            LOGD("%d", char(scs.subsession->clientPortNum()+ 1));
        }
        scs.subsession->sink = VideoSink::createNew(env, *scs.subsession,
                                                    rtspClient->url(),((MyRtspClient*) rtspClient)->frameCallback);
        // perhaps use your own custom "MediaSink" subclass instead
        if (scs.subsession->sink == NULL) {
            LOGI(
                    "Failed to create a data sink for the:%s ", env.getResultMsg());
            break;
        }
        scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
        scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                                           subsessionAfterPlaying, scs.subsession);
        if (scs.subsession->rtcpInstance() != NULL) {
            scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler,
                                                          scs.subsession);
        }
    } while (0);
    delete[] resultString;
    setupNextSubsession(rtspClient);
}

void Rtsp::continueAfterPLAY(RTSPClient* rtspClient, int resultCode,
                       char* resultString) {
    LOGI("continueAfterPLAY");
    Boolean success = False;

    do {
        UsageEnvironment& env = rtspClient->envir(); // alias
        StreamClientState& scs = ((MyRtspClient*) rtspClient)->scs; // alias

        if (resultCode != 0) {
            LOGD("Failed to start playing session: %s", resultString);
            break;
        }

        if (scs.duration > 0) {
            unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
            scs.duration += delaySlop;
            unsigned uSecsToDelay = (unsigned) (scs.duration * 1000000);
            scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(
                    uSecsToDelay, (TaskFunc*) streamTimerHandler, rtspClient);
        }

        LOGI("Started playing session");
        if (scs.duration > 0) {
            LOGI( " for up to %f seconds", scs.duration);
        }
        success = True;
    } while (0);
    delete[] resultString;

    if (!success) {
        // An unrecoverable error occurred with this stream.
        shutdownStream(rtspClient);
    }else{
        ((MyRtspClient*) rtspClient)->isPlaying = True;
    }

}

void Rtsp::subsessionAfterPlaying(void* clientData) {
    LOGI("subsessionAfterPlaying");
    MediaSubsession* subsession = (MediaSubsession*) clientData;
    RTSPClient* rtspClient = (RTSPClient*) (subsession->miscPtr);

    // Begin by closing this subsession's stream:
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    // Next, check whether *all* subsessions' streams have now been closed:
    MediaSession& session = subsession->parentSession();
    MediaSubsessionIterator iter(session);
    while ((subsession = iter.next()) != NULL) {
        if (subsession->sink != NULL)
            return; // this subsession is still active
    }

    // All subsessions' streams have now been closed, so shutdown the client:
    shutdownStream(rtspClient);
}

void Rtsp::subsessionByeHandler(void* clientData) {
    LOGI("subsessionByeHandler");
    MediaSubsession* subsession = (MediaSubsession*) clientData;
    RTSPClient* rtspClient = (RTSPClient*) subsession->miscPtr;
    UsageEnvironment& env = rtspClient->envir(); // alias
    LOGI("Received RTCP \"BYE\" on \"");
    // Now act as if the subsession had closed:
    subsessionAfterPlaying(subsession);
}

void Rtsp::streamTimerHandler(void* clientData) {
    LOGI("streamTimerHandler");
    MyRtspClient* rtspClient = (MyRtspClient*) clientData;
    StreamClientState& scs = rtspClient->scs; // alias

    scs.streamTimerTask = NULL;
    // Shut down the stream:
    shutdownStream(rtspClient);
}

bool Rtsp::isPlaying() {
    return rtsp_client->isPlaying;
}