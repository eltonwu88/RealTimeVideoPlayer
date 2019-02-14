#ifndef MAIN_APP_AND_LIBRARY_LOG_H
#define MAIN_APP_AND_LIBRARY_LOG_H

#include <android/log.h>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,"UdpLive",__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,"UdpLive",__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,"UdpLive",__VA_ARGS__)


#endif //MAIN_APP_AND_LIBRARY_LOG_H
