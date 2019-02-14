package com.example.eltonwu.udplive;

import android.view.Surface;

public class UDPLive {
    @SuppressWarnings("unused")
    private byte[] nv21array = null;
    @SuppressWarnings("unused")
    private byte[] argbarray = null;

    @SuppressWarnings("unused")
    private static final String TAG = "UDPLive";
    private static UDPLive sInstance = null;

    static {
        System.loadLibrary("native-lib");
    }

    public static UDPLive getInstance(){
        if(sInstance == null){
            sInstance = new UDPLive();
        }
        return sInstance;
    }
    private UDPLive(){}

    private native void init();
    private native void deinit();
    public native void setSurface(Surface surface);

    public interface PreviewListener{
        void onFrame(byte[] data,int width,int height,int format);
    }
    private PreviewListener mPreviewListener;
    void setPreviewListener(PreviewListener listener){
        mPreviewListener = listener;
    }

    public void start(){
        init();
    }
    public void stop(){
        deinit();
    }

    //format 1:nv21,2:argb
    @SuppressWarnings("unused")
    private void onFrame(int format,byte[] data,int width,int height){
        if(mPreviewListener != null){
            mPreviewListener.onFrame(data,width,height,format);
        }
    }
}
