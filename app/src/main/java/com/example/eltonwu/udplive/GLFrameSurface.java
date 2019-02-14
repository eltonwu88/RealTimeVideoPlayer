package com.example.eltonwu.udplive;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;


public class GLFrameSurface extends GLSurfaceView {
    private Renderer renderer;

    public GLFrameSurface(Context context) {
        super(context);
    }

    public GLFrameSurface(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        // setRenderMode() only takes effectd after SurfaceView attached to window!
        // note that on this mode, surface will not render util GLSurfaceView.requestRender() is
        // called, it's good and efficient -v-
        setRenderMode(RENDERMODE_WHEN_DIRTY);
    }


    @Override
    public void setRenderer(Renderer renderer) {
        this.renderer = renderer;

        super.setEGLContextClientVersion(2);
        super.setRenderer(renderer);
    }

    public void setRenderer(GLFrameRenderer renderer) {
        this.renderer = renderer;

        super.setEGLContextClientVersion(2);

        super.setRenderer(renderer);
    }

    public boolean hasRenderer() {
        return this.renderer!=null;
    }

//    public GLFrameRenderer getRenderer() {
//        return renderer;
//    }
}
