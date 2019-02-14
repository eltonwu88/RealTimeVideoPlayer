#include <sys/time.h>
#include <cstring>
#include "log.h"
#include "FramePacket.h"

FramePacket::FramePacket(uint8_t *data,size_t length,int width,int height,int colorFormat,bool nv21notify,bool drawByUser){
    this->data = static_cast<uint8_t *>(malloc(length));
    this->length = length;
    if(this->data == NULL){
        LOGE("failed to alloc frame");
        return;
    }
    memcpy(this->data,data,length);
    gettimeofday(&timestamp,NULL);

    this->width = width;
    this->height= height;
    this->colorFormat = colorFormat;

    this->nv21notify = nv21notify;
    this->drawByUser = drawByUser;
}
FramePacket::~FramePacket() {
    if(data != NULL){
        free(data);
        data = NULL;
    }
}

FramePacket::FramePacket(const FramePacket &src) {
    this->data = static_cast<uint8_t *>(malloc(src.length));
    this->length = src.length;
    if(this->data == NULL){
        LOGE("failed to alloc packet");
        return;
    }
    memcpy(this->data,src.data,length);
    this->timestamp = src.timestamp;
    this->colorFormat = src.colorFormat;
    this->width = src.width;
    this->height= src.height;

    this->drawByUser = src.drawByUser;
    this->nv21notify = src.nv21notify;
}