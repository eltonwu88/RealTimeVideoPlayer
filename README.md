# Real Time Video player Demo
### using rtsp/udp protocol to transfer H264 data
### using android MediaCodec to decode H264 frame
### using opengles to render decoded frame into screen
  
  
### in this demo,it uses udp to transfer H264 data by default,you can switch to rtsp protocol transfer by modifing some code
  
### in this demo,it provides two ways to render into screen since various demands,that is NV21（by default） or ARGB.
  
## TODO
### unify udp and rtsp protocol,extract a abstract interface to transfer H264 data, switching udp/rtsp by config file instead of modifing code
