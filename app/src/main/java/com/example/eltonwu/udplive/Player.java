package com.example.eltonwu.udplive;

import android.opengl.GLSurfaceView;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import com.example.eltonwu.udplive.gles.VideoRender;

import java.util.Timer;
import java.util.TimerTask;

public class Player implements UDPLive.PreviewListener, SurfaceHolder.Callback {
    private UDPLive mUdpLive = UDPLive.getInstance();
    private SurfaceView  mSurfaceView;

    private VideoRender mRender;
//    private GLFrameRenderer mRender;

    private Timer mTimer;

    public void init(){
        mUdpLive.start();
        mUdpLive.setPreviewListener(this);
    }

    public void deinit(){
        mUdpLive.stop();
        mUdpLive.setPreviewListener(null);

        if(mSurfaceView != null){
            mSurfaceView.getHolder().removeCallback(this);
        }

        if(mTimer != null){
            mTimer.cancel();
            mTimer = null;
        }
    }

    public void setSurfaceView(SurfaceView surfaceView){
        if(mSurfaceView != surfaceView){
            mSurfaceView = surfaceView;
            if(mSurfaceView instanceof GLSurfaceView){
                GLFrameSurface glv = (GLFrameSurface) mSurfaceView;
                mRender = new VideoRender();
//                mRender = new GLFrameRenderer(glv);
                glv.setRenderer(mRender);
            }
            mSurfaceView.getHolder().addCallback(this);
        }
    }

    @Override
    public void onFrame(final byte[] data,int width,int height,int colorFormat) {
        synchronized (this){
            if(mSurfaceView instanceof GLSurfaceView){
                mRender.onPreviewFrame(data,width,height);
//                mRender.update(data,width,height,21);
                GLFrameSurface surface = (GLFrameSurface) mSurfaceView;
                surface.requestRender();


            }
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mUdpLive.setSurface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
    }

    private boolean isPaused = false;
    public void onActivityResume(){
        if(mSurfaceView instanceof GLSurfaceView){
            GLFrameSurface glv = (GLFrameSurface) mSurfaceView;
            if(isPaused){
//                glv.setEGLContextClientVersion(2);
                glv.onResume();
            }
            isPaused = false;
        }
    }
    public void onActivityPause(){
        if(mSurfaceView instanceof GLSurfaceView){
            GLFrameSurface glv = (GLFrameSurface) mSurfaceView;
            if(!isPaused){
                glv.onPause();
            }
            isPaused = true;
        }
    }
}
