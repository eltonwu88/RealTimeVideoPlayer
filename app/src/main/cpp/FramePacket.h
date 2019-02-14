#ifndef UDPLIVE_FRAMEPACKET_H
#define UDPLIVE_FRAMEPACKET_H
#include <stdint.h>

class FramePacket {
public:
    FramePacket(uint8_t *data,size_t length,int width,int height,int colorFormat,bool nv21notify,bool drawByUser);
    FramePacket(const FramePacket &src);
    ~FramePacket();

    uint8_t *data;
    size_t  length;
    int     width;
    int     height;
    int     colorFormat;

    bool    nv21notify;
    bool    drawByUser;

    struct timeval    timestamp;
};


#endif //UDPLIVE_FRAMEPACKET_H
