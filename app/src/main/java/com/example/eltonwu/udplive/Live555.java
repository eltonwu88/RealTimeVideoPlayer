package com.example.eltonwu.udplive;

import android.util.Log;
import android.view.Surface;

public class Live555 {
    @SuppressWarnings("unused")
    private byte[] nv21array = null;
    @SuppressWarnings("unused")
    private byte[] argbarray = null;

    @SuppressWarnings("unused")
    private static final String TAG = "Live555";
    private static Live555 sInstance = null;

    static {
        System.loadLibrary("gnustl_shared");
        System.loadLibrary("live555");
        System.loadLibrary("native-lib");
    }

    public static Live555 getInstance(){
        if(sInstance == null){
            sInstance = new Live555();
        }
        return sInstance;
    }
    private Live555(){}

    private native void init(String url);
    private native void deinit();
    public native void setSurface(Surface surface);
    public native void nv21notify(boolean enable);
    public native void drawByUser(boolean enable);
    public native boolean isPlaying();

    public interface PreviewListener{
        void onFrame(byte[] data, int width, int height, int format);
    }
    private PreviewListener mPreviewListener;
    public void setPreviewListener(PreviewListener listener){
        mPreviewListener = listener;
    }

    public void start(String url){
        if(!isPlaying()){
            init(url);
        }else{
            Log.w(TAG,"player is already running");
        }

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
