/*
 * Copyright (C) 2012 CyberAgent
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.eltonwu.udplive.gles;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView.Renderer;
import android.os.Handler;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class VideoRender implements Renderer {
    public  static final int NO_IMAGE = -1;
    public static final float CUBE[] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f,
    };
    private boolean maintainAspect;

    private final Object mSurfaceChangedWaiter = new Object();

    private int mGLTextureId = NO_IMAGE;
    private final FloatBuffer mGLCubeBuffer;
    private final FloatBuffer mGLTextureBuffer;

    private int mOutputWidth;
    private int mOutputHeight;
    private int mImageWidth;
    private int mImageHeight;
    private ByteBuffer mGLRgbBuffer;

    private Rotation mRotation;
    private boolean mFlipHorizontal;
    private boolean mFlipVertical;

    private final Queue<Runnable> mRunOnDraw = new LinkedList<>();
    private RenderProgram mRenderProgram = new RenderProgram();

    private Handler mHandler = new Handler();
    private int     numFrames,numDropFrames= 0;
    private Lock lock = new ReentrantLock();

    private Runnable mFPSRunnable = new Runnable() {
        @Override
        public void run() {
            Log.i("TEST","test fps :"+numFrames+" drop :"+numDropFrames);
            numFrames = 0;
            numDropFrames = 0;
            mHandler.postDelayed(mFPSRunnable,1000);
        }
    };

    public VideoRender() {
        mGLCubeBuffer = ByteBuffer.allocateDirect(CUBE.length * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer();
        mGLCubeBuffer.put(CUBE).position(0);

        mGLTextureBuffer = ByteBuffer.allocateDirect(TextureRotationUtil.TEXTURE_NO_ROTATION.length * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer();
        setRotation(Rotation.NORMAL, false, false);
        mHandler.postDelayed(mFPSRunnable,3000);
    }

    @Override
    public void onSurfaceCreated(final GL10 unused, final EGLConfig config) {
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);
        GLES20.glDisable(GLES20.GL_DEPTH_TEST);
        mRenderProgram.init();
    }

    @Override
    public void onSurfaceChanged(final GL10 gl, final int width, final int height) {
        mOutputWidth = width;
        mOutputHeight = height;
        GLES20.glViewport(0, 0, width, height);
        GLES20.glUseProgram(mRenderProgram.getProgram());
        mRenderProgram.onOutputSizeChanged(width, height);
        adjustImageScaling();
        synchronized (mSurfaceChangedWaiter) {
            mSurfaceChangedWaiter.notifyAll();
        }
    }

    private void runOnDraw(final Runnable runnable) {
        synchronized (mRunOnDraw) {
            mRunOnDraw.add(runnable);
        }
    }
    private void runAll(Queue<Runnable> queue) {
        while (!queue.isEmpty()) {
            queue.poll().run();
        }
    }

    private long lastDrawCost = -1;
    @Override
    public void onDrawFrame(final GL10 gl) {

        long now = System.currentTimeMillis();
        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);
        lock.lock();
        runAll(mRunOnDraw);
        mRenderProgram.onDraw(mGLTextureId, mGLCubeBuffer, mGLTextureBuffer);
        lock.unlock();
        lastDrawCost = System.currentTimeMillis() - now;
        numFrames ++;
    }

    public void onPreviewFrame(byte[] data, final int width,final int height) {
        if(mGLRgbBuffer == null){
            mGLRgbBuffer = ByteBuffer.allocateDirect(data.length);
        }else{
            if(mGLRgbBuffer.capacity() != data.length){
                mGLRgbBuffer = ByteBuffer.allocateDirect(data.length);
            }
        }
        lock.lock();
        mGLRgbBuffer.clear();
        mGLRgbBuffer.put(data);
        mGLRgbBuffer.position(0);
        lock.unlock();

        if (mRunOnDraw.isEmpty()) {
            runOnDraw(new Runnable() {
                @Override
                public void run() {
                    mGLTextureId = OpenGlUtils.loadTexture(mGLRgbBuffer,width,height,mGLTextureId);
                    if (mImageWidth != width) {
                        mImageWidth = width;
                        mImageHeight = height;
                        adjustImageScaling();
                    }
                }
            });
        }else{
            numDropFrames ++;
        }

    }


    private void adjustImageScaling() {
        float outputWidth = mOutputWidth;
        float outputHeight = mOutputHeight;
        if (mRotation == Rotation.ROTATION_270 || mRotation == Rotation.ROTATION_90) {
            outputWidth = mOutputHeight;
            outputHeight = mOutputWidth;
        }

        float ratio1 = outputWidth / mImageWidth;
        float ratio2 = outputHeight / mImageHeight;

        int imageWidthNew,imageHeightNew;
        if(maintainAspect){
            float ratioMax = Math.max(ratio1, ratio2);
            imageWidthNew = Math.round(mImageWidth * ratioMax);
            imageHeightNew = Math.round(mImageHeight * ratioMax);
        }else{
            imageWidthNew = Math.round(mImageWidth * ratio1);
            imageHeightNew = Math.round(mImageHeight * ratio2);
        }

        float ratioWidth = imageWidthNew / outputWidth;
        float ratioHeight = imageHeightNew / outputHeight;

        float[] cube = CUBE;
        float[] textureCords = TextureRotationUtil.getRotation(mRotation, mFlipHorizontal, mFlipVertical);
        cube = new float[]{
                CUBE[0] / ratioHeight, CUBE[1] / ratioWidth,
                CUBE[2] / ratioHeight, CUBE[3] / ratioWidth,
                CUBE[4] / ratioHeight, CUBE[5] / ratioWidth,
                CUBE[6] / ratioHeight, CUBE[7] / ratioWidth,
        };
        mGLCubeBuffer.clear();
        mGLCubeBuffer.put(cube).position(0);
        mGLTextureBuffer.clear();
        mGLTextureBuffer.put(textureCords).position(0);
    }

    public void setRotation(final Rotation rotation) {
        mRotation = rotation;
        adjustImageScaling();
    }

    public void setRotation(final Rotation rotation,
                            final boolean flipHorizontal, final boolean flipVertical) {
        mFlipHorizontal = flipHorizontal;
        mFlipVertical = flipVertical;
        setRotation(rotation);
    }

    public Rotation getRotation() {
        return mRotation;
    }

    public boolean isFlippedHorizontally() {
        return mFlipHorizontal;
    }

    public boolean isFlippedVertically() {
        return mFlipVertical;
    }
}
