package com.example.eltonwu.udplive;

import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.GLSurfaceView.Renderer;
import android.util.Log;

import java.nio.ByteBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;
import java.util.concurrent.locks.*;


class GLFrameRenderer implements Renderer {
    private static final String TAG = "GLFrameRenderer";
    private GLSurfaceView mTargetSurface;
    private GLProgram prog = new GLProgram(0);
    private static Lock lock = new ReentrantLock();
    private ByteBuffer y;
    private ByteBuffer uv;

    private ByteBuffer swapY;
    private ByteBuffer swapUV;

    private int orderVU;
    private int type;

    private int screenWidth;
    private int screenHeight;

    private int yHorizontalStride;
    private int uvHorizontalStride;
    private int verticalStride;

    private int lrSpace;
    private int cSpace;

    private boolean vrMode = false;
    private byte pictureFlag = 0;

    int mFPS;

    GLFrameRenderer(GLSurfaceView surface) {
        mTargetSurface = surface;
        lrSpace = 0;
        cSpace = 0;
    }

    @Override
    public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        

        if (!prog.isProgramBuilt()) {
            prog.buildProgram();
        }
    }

    @Override
    public void onSurfaceChanged(GL10 gl, int width, int height) {
        Log.d(TAG, "onSurfaceChanged of width: " + width + ", height:" + height);
//        GLES20.glViewport(0, 0, width, height);
        this.screenWidth = width;
        this.screenHeight = height;
    }

    @Override
    public void onDrawFrame(GL10 gl) {
//        Log.d(TAG, "onDrawFrame start");
        long startTime = System.nanoTime(); 

        
        if (y != null) {
            // reset position, have to be done
             lock.lock();
            y.position(0);
            uv.position(0);

            prog.buildTextures(y, uv, type, yHorizontalStride, uvHorizontalStride, verticalStride);
            GLES20.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT );

            if(vrMode) {
//		  Log.d(TAG, "VRMode screenWidth: " + screenWidth + ", screenHeight:" + screenHeight);
                GLES20.glViewport(lrSpace, screenHeight / 4, screenWidth / 2 - lrSpace - cSpace, screenHeight / 2);
                prog.drawFrame(orderVU, type);
                GLES20.glViewport(screenWidth / 2 + cSpace, screenHeight / 4, screenWidth / 2 - lrSpace - cSpace, screenHeight / 2);
                prog.drawFrame(orderVU, type);
            }
            else {
                GLES20.glViewport(0, 0, screenWidth, screenHeight);
//                Log.d(TAG, "screenWidth: " + screenWidth + ", screenHeight:" + screenHeight);
                 prog.drawFrame(orderVU, type);
            }

            y.clear();
            uv.clear();
	     lock.unlock();
//	     long consumingTime =System.nanoTime()-startTime;
//            System.out.println(consumingTime/1000+" onDrawFrame");
        }
//        Log.d(TAG, "onDrawFrame end");
	    mFPS ++;
    }

    /**
     * this method will be called from native code, it happens when the video is about to play or
     * the video size changes.
     */
    public void update(int w, int h) {
        Log.d(TAG, "update of width: " + w + ", height:" + h);
        if (w > 0 && h > 0) {
            // 初始化容器
            //TODO:
          
           

        }
    }

    /**
     * this method will be called from native code, it's used for passing yuv data to me.
     */
    void update(byte[] data, int width, int height, int colorFormat) {
        int yVerticalSpan = height;
        int uvVerticalSpan = height;
        verticalStride = height;
//	 Log.d(TAG, "zmc update");
        switch (colorFormat) {
            case 19://COLOR_FormatYUV420Planar
                type = 0;
                orderVU = 0;
                yHorizontalStride = width;
                yVerticalSpan = height;
                uvHorizontalStride = yHorizontalStride/2;
                uvVerticalSpan = yVerticalSpan;
                break;
            case 21://COLOR_FormatYUV420SemiPlanar
                type = 1;
                orderVU = 0;
                yHorizontalStride = width;
                yVerticalSpan = height;
                uvHorizontalStride = yHorizontalStride;
                uvVerticalSpan = yVerticalSpan/2;
                break;
            case 0x7FA30C03://OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka
                type = 1;
                orderVU = 1;
                int hAlign = 64;
                int vAlign = 32;
                yHorizontalStride = width%hAlign>0 ? hAlign*(width/hAlign + 1) : width;
                yVerticalSpan = height%vAlign>0 ? vAlign*(height/vAlign + 1) : height;
                uvHorizontalStride = yHorizontalStride;
                uvVerticalSpan = yVerticalSpan/2;
                break;
            case 0x7FA30C04://COLOR_QCOM_FormatYUV420SemiPlanar32m
                type = 1;
                orderVU = 0;
                hAlign = 128;
                vAlign = 32;
                yHorizontalStride = width%hAlign>0 ? hAlign*(width/hAlign + 1) : width;
                yVerticalSpan = height%vAlign>0 ? vAlign*(height/vAlign + 1) : height;
                uvHorizontalStride = yHorizontalStride;
                uvVerticalSpan = yVerticalSpan/2;
                break;
	       default:
		        type = 1;
                orderVU = 0;
                hAlign = 128;
                vAlign = 32;
                yHorizontalStride = width%hAlign>0 ? hAlign*(width/hAlign + 1) : width;
                yVerticalSpan = height%vAlign>0 ? vAlign*(height/vAlign + 1) : height;
                uvHorizontalStride = yHorizontalStride;
                uvVerticalSpan = yVerticalSpan/2;
		  break;	
	
        }
	
        int ySize = yHorizontalStride * yVerticalSpan;
        int uvSize = uvHorizontalStride * uvVerticalSpan;
        if(swapY == null){
            swapY = ByteBuffer.allocateDirect(ySize);
        }else{
            if(swapY.capacity() != ySize){
                swapY = ByteBuffer.allocateDirect(ySize);
            }
        }
        if(swapUV == null){
            swapUV = ByteBuffer.allocateDirect(uvSize);
        }else{
            if(swapUV.capacity() != uvSize){
                swapUV = ByteBuffer.allocateDirect(uvSize);
            }
        }

        lock.lock();
        swapY.clear();
        swapUV.clear();
        swapY.put(data,0,ySize);
        swapUV.put(data,ySize,uvSize);
        this.y=swapY;
        this.uv=swapUV;
        lock.unlock();

	
//	 Log.d(TAG, "zmc requestRender yHorizontalStride: "+yHorizontalStride+"yVerticalSpan:"+yVerticalSpan);
//	 Log.d(TAG, "zmc uvHorizontalStride:"+uvHorizontalStride+"uvVerticalSpan:"+uvVerticalSpan);
        // request to render
        mTargetSurface.requestRender();
    }

    void setVRMode(boolean enable){
        vrMode = enable;
    }

    void setMirror(){
        if(prog != null){
            prog.setVerticalFlip();
        }else{
            pictureFlag |= 0x01;
        }

    }
    void setHorizontalFlip(){
        if(prog != null){
            prog.setHorizontalFlip();
        }else{
            pictureFlag |= 0x10;
        }
    }

    void setMirrorAndHorizontalFlip(){
        if(prog != null){
            prog.setMirrorAndHorizontalFlip();
        }else{
            pictureFlag |= 0x11;
        }
    }
}
