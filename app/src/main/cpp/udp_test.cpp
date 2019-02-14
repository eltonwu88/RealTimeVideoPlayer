#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include "log.h"

static void *workThread(void *arg){
    int fd = socket(AF_INET,SOCK_DGRAM,0);
    if(fd <= 0){
        LOGE("create udp fails %s",strerror(errno));
        return NULL;
    }

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(6236);
    address.sin_addr.s_addr = inet_addr("192.168.100.1");

//    LOGI("bind udp port");
//    if(bind(fd,(const sockaddr *)&address,sizeof(address)) < 0){
//        LOGE("socket bind fail %s",strerror(errno));
//        return NULL;
//    }
    LOGI("send to server");
    const char *data = "aloha";
    if(sendto(fd,data,strlen(data),0,(const sockaddr *)&address,sizeof(address)) < 0){
        LOGI("send aloha failed %s",strerror(errno));
    }

    struct sockaddr_in incoming = {0};
    size_t incoming_len = sizeof(incoming);
    bzero(&incoming,sizeof(incoming));
    unsigned char buffer[256];
    memset(buffer,0,sizeof(buffer));
    while(1){
        LOGI("receiving...");
        int len = (int) recvfrom(fd,buffer,sizeof(buffer),0,(sockaddr *)&incoming,(socklen_t *)&incoming_len);
        if(len < 0){
            LOGE("recv fail %s",strerror(errno));
            break;
        }
        LOGI("recv : %s",buffer);
        memset(buffer,0,sizeof(buffer));
    }
    return NULL;
}

void udp_test(void){
    pthread_t tid;
    pthread_create(&tid,NULL,workThread,NULL);
}
