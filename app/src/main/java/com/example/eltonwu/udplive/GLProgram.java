package com.example.eltonwu.udplive;

import android.opengl.GLES20;
import android.util.Log;

import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;


/**
 * step to use:<br/>
 * 1. new GLProgram()<br/>
 * 2. buildProgram()<br/>
 * 3. buildTextures()<br/>
 * 4. drawFrame()<br/>
 */
class GLProgram {
    private static final String TAG = "GLProgram";
    // program id
    private int _program;
    // window position
    private final int mWinPosition;
    // texture id
    private int _textureI;
    private int _textureII;
    private int _textureIII;
    // texture index in gles
    private int _tIindex;
    private int _tIIindex;
    private int _tIIIindex;
    // vertices on screen
    private float[] _vertices;
    // handles
    private int _positionHandle = -1, _coordHandle = -1;
    private int _yhandle = -1, _uvhandle = -1;
    private int _orderVU = -1, _type = -1;
    private int _ytid = -1, _uvtid = -1;
    private int yHorizontalStride;
    private int verticalStride;
    // vertices buffer
    private ByteBuffer _vertice_buffer;
    private ByteBuffer _coord_buffer;
    // flow control
    private boolean isProgBuilt = false;

    /**
     * position can only be 0~4:<br/>
     * fullscreen => 0<br/>
     * left-top => 1<br/>
     * right-top => 2<br/>
     * left-bottom => 3<br/>
     * right-bottom => 4
     */
    GLProgram(int position) {
        if (position < 0 || position > 4) {
            throw new RuntimeException("Index can only be 0 to 4");
        }
        mWinPosition = position;
        setup(mWinPosition);
    }

    /**
     * prepared for later use
     */
    private void setup(int position) {
        switch (mWinPosition) {
            case 1:
                _vertices = squareVertices1;
                _textureI = GLES20.GL_TEXTURE0;
                _textureII = GLES20.GL_TEXTURE1;
                _textureIII = GLES20.GL_TEXTURE2;
                _tIindex = 0;
                _tIIindex = 1;
                _tIIIindex = 2;
                break;
            case 2:
                _vertices = squareVertices2;
                _textureI = GLES20.GL_TEXTURE3;
                _textureII = GLES20.GL_TEXTURE4;
                _textureIII = GLES20.GL_TEXTURE5;
                _tIindex = 3;
                _tIIindex = 4;
                _tIIIindex = 5;
                break;
            case 3:
                _vertices = squareVertices3;
                _textureI = GLES20.GL_TEXTURE6;
                _textureII = GLES20.GL_TEXTURE7;
                _textureIII = GLES20.GL_TEXTURE8;
                _tIindex = 6;
                _tIIindex = 7;
                _tIIIindex = 8;
                break;
            case 4:
                _vertices = squareVertices4;
                _textureI = GLES20.GL_TEXTURE9;
                _textureII = GLES20.GL_TEXTURE10;
                _textureIII = GLES20.GL_TEXTURE11;
                _tIindex = 9;
                _tIIindex = 10;
                _tIIIindex = 11;
                break;
            case 0:
            default:
                _vertices = squareVertices;
                _textureI = GLES20.GL_TEXTURE0;
                _textureII = GLES20.GL_TEXTURE1;
                _textureIII = GLES20.GL_TEXTURE2;
                _tIindex = 0;
                _tIIindex = 1;
                _tIIIindex = 2;
                break;
        }
    }

    boolean isProgramBuilt() {
        return isProgBuilt;
    }

    void buildProgram() {
        createBuffers(_vertices);

        if (_program <= 0) {
            _program = createProgram(VERTEX_SHADER, FRAGMENT_SHADER);
        }
        Log.d(TAG, "_program = " + _program);

        /*
         * get handle for "vPosition" and "a_texCoord"
         */
        _positionHandle = GLES20.glGetAttribLocation(_program, "vPosition");
        Log.d(TAG, "_positionHandle = " + _positionHandle);
        checkGlError("glGetAttribLocation vPosition");
        if (_positionHandle == -1) {
            throw new RuntimeException("Could not get attribute location for vPosition");
        }
        _coordHandle = GLES20.glGetAttribLocation(_program, "a_texCoord");
        Log.d(TAG, "_coordHandle = " + _coordHandle);
        checkGlError("glGetAttribLocation a_texCoord");
        if (_coordHandle == -1) {
            throw new RuntimeException("Could not get attribute location for a_texCoord");
        }

        /*
         * get uniform location for y/u/v, we pass data through these uniforms
         */
        _yhandle = GLES20.glGetUniformLocation(_program, "tex_y");
        Log.d(TAG, "_yhandle = " + _yhandle);
        checkGlError("glGetUniformLocation tex_y");
        if (_yhandle == -1) {
            throw new RuntimeException("Could not get uniform location for tex_y");
        }

        _uvhandle = GLES20.glGetUniformLocation(_program, "tex_uv");
        Log.d(TAG, "_uvhandle = " + _uvhandle);
        checkGlError("glGetUniformLocation tex_uv");
        if (_uvhandle == -1) {
            throw new RuntimeException("Could not get uniform location for tex_uv");
        }

        _orderVU = GLES20.glGetUniformLocation(_program, "orderVU");
        Log.d(TAG, "_orderVU = " + _orderVU);
        checkGlError("glGetUniformLocation orderVU");
        if (_orderVU == -1) {
            throw new RuntimeException("Could not get uniform location for orderVU");
        }

        _type = GLES20.glGetUniformLocation(_program, "type");
        Log.d(TAG, "_type = " + _type);
        checkGlError("glGetUniformLocation type");
        if (_type == -1) {
            throw new RuntimeException("Could not get uniform location for type");
        }

        isProgBuilt = true;
    }

    /**
     * build a set of textures, one for R, one for G, and one for B.
     */
    void buildTextures(Buffer y, Buffer uv, int type, int yHorizontalStride, int uvHorizontalStride, int verticalStride) {
        boolean videoSizeChanged = (this.yHorizontalStride != yHorizontalStride || this.verticalStride != verticalStride);
        if (videoSizeChanged) {
            this.yHorizontalStride = yHorizontalStride;
            this.verticalStride = verticalStride;
            Log.d(TAG, "buildTextures videoSizeChanged: w=" + yHorizontalStride + " h=" + verticalStride);
        }

        // building texture for Y data
        if (_ytid < 0 || videoSizeChanged) {
            if (_ytid >= 0) {
                Log.d(TAG, "glDeleteTextures Y");
                GLES20.glDeleteTextures(1, new int[] { _ytid }, 0);
                checkGlError("glDeleteTextures");
            }
            // GLES20.glPixelStorei(GLES20.GL_UNPACK_ALIGNMENT, 1);
            int[] textures = new int[1];
            GLES20.glGenTextures(1, textures, 0);
            checkGlError("glGenTextures");
            _ytid = textures[0];
            Log.d(TAG, "glGenTextures Y = " + _ytid);
        }
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, _ytid);
        checkGlError("glBindTexture");
       GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, yHorizontalStride, verticalStride, 0,
                GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, y);
        checkGlError("glTexImage2D");
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);

        // building texture for U data
        if (_uvtid < 0 || videoSizeChanged) {
            if (_uvtid >= 0) {
                Log.d(TAG, "glDeleteTextures U");
                GLES20.glDeleteTextures(1, new int[] { _uvtid }, 0);
                checkGlError("glDeleteTextures");
            }
            int[] textures = new int[1];
            GLES20.glGenTextures(1, textures, 0);
            checkGlError("glGenTextures");
            _uvtid = textures[0];
            Log.d(TAG, "glGenTextures U = " + _uvtid);
        }
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, _uvtid);
        if (type==1) {//semiplanar
           GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE_ALPHA, uvHorizontalStride / 2, verticalStride / 2, 0,
                    GLES20.GL_LUMINANCE_ALPHA, GLES20.GL_UNSIGNED_BYTE, uv);
        } else {//planar
           GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, uvHorizontalStride, verticalStride, 0, GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, uv);
        }
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_NEAREST);
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE);
    }

    /**
     * render the frame
     * the YUV data will be converted to RGB by shader.
     */
    void drawFrame(int orderVU, int type) {
    	Log.d(TAG, "drawFrame");
        GLES20.glUseProgram(_program);
        checkGlError("glUseProgram");

        GLES20.glVertexAttribPointer(_positionHandle, 2, GLES20.GL_FLOAT, false, 8, _vertice_buffer);
        checkGlError("glVertexAttribPointer mPositionHandle");
        GLES20.glEnableVertexAttribArray(_positionHandle);

        GLES20.glVertexAttribPointer(_coordHandle, 2, GLES20.GL_FLOAT, false, 8, _coord_buffer);
        checkGlError("glVertexAttribPointer maTextureHandle");
        GLES20.glEnableVertexAttribArray(_coordHandle);

        GLES20.glUniform1i(_orderVU, orderVU);
        GLES20.glUniform1i(_type, type);

        // bind textures
        GLES20.glActiveTexture(_textureI);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, _ytid);
        GLES20.glUniform1i(_yhandle, _tIindex);

        GLES20.glActiveTexture(_textureII);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, _uvtid);
        GLES20.glUniform1i(_uvhandle, _tIIindex);

        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        GLES20.glFinish();

        GLES20.glDisableVertexAttribArray(_positionHandle);
        GLES20.glDisableVertexAttribArray(_coordHandle);
    }

    /**
     * create program and load shaders, fragment shader is very important.
     */
    private int createProgram(String vertexSource, String fragmentSource) {
        // create shaders
        int vertexShader = loadShader(GLES20.GL_VERTEX_SHADER, vertexSource);
        int pixelShader = loadShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
	
        // just check
    
		Log.d(TAG, "vertexShader = " + vertexShader);
              Log.d(TAG, "pixelShader = " + pixelShader);


        int program = GLES20.glCreateProgram();
        if (program != 0) {
            GLES20.glAttachShader(program, vertexShader);
            checkGlError("glAttachShader");
            GLES20.glAttachShader(program, pixelShader);
            checkGlError("glAttachShader");
            GLES20.glLinkProgram(program);
            int[] linkStatus = new int[1];
            GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
            if (linkStatus[0] != GLES20.GL_TRUE) {
                Log.e(TAG, "Could not link program: ");
                Log.e(TAG, GLES20.glGetProgramInfoLog(program));
                GLES20.glDeleteProgram(program);
                program = 0;
            }
        }
        return program;
    }

    /**
     * create shader with given source.
     */
    private int loadShader(int shaderType, String source) {
        int shader = GLES20.glCreateShader(shaderType);
        if (shader != 0) {
            GLES20.glShaderSource(shader, source);
            GLES20.glCompileShader(shader);
            int[] compiled = new int[1];
            GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compiled, 0);
            if (compiled[0] == 0) {
                Log.e(TAG, "Could not compile shader " + shaderType);
                Log.e(TAG, GLES20.glGetShaderInfoLog(shader));
                GLES20.glDeleteShader(shader);
                shader = 0;
            }
        }
        return shader;
    }

    /**
     * these two buffers are used for holding vertices, screen vertices and texture vertices.
     */
    private void createBuffers(float[] vert) {
        _vertice_buffer = ByteBuffer.allocateDirect(vert.length * 4);
        _vertice_buffer.order(ByteOrder.nativeOrder());
        _vertice_buffer.asFloatBuffer().put(vert);
        _vertice_buffer.position(0);

        if (_coord_buffer == null) {
            _coord_buffer = ByteBuffer.allocateDirect(coordVertices.length * 4);
            _coord_buffer.order(ByteOrder.nativeOrder());
            _coord_buffer.asFloatBuffer().put(coordVertices);
            _coord_buffer.position(0);
        }
    }

    private void checkGlError(String op) {
        int error;
        while ((error = GLES20.glGetError()) != GLES20.GL_NO_ERROR) {
            Log.e(TAG, "***** " + op + ": glError " + error);
            throw new RuntimeException(op + ": glError " + error);
        }
    }

    void setVerticalFlip(){
        if (_coord_buffer == null) {
            _coord_buffer = ByteBuffer.allocateDirect(coorVerticesFV.length * 4);
            _coord_buffer.order(ByteOrder.nativeOrder());
            _coord_buffer.asFloatBuffer().put(coorVerticesFV);
            _coord_buffer.position(0);
        }else{
            _coord_buffer.clear();
            _coord_buffer.asFloatBuffer().put(coorVerticesFV);
            _coord_buffer.position(0);
        }
    }

    void setHorizontalFlip(){
        if (_coord_buffer == null) {
            _coord_buffer = ByteBuffer.allocateDirect(coorVerticesFH.length * 4);
            _coord_buffer.order(ByteOrder.nativeOrder());
            _coord_buffer.asFloatBuffer().put(coorVerticesFH);
            _coord_buffer.position(0);
        }else{
            _coord_buffer.clear();
            _coord_buffer.asFloatBuffer().put(coorVerticesFH);
            _coord_buffer.position(0);
        }
    }
    void setMirrorAndHorizontalFlip(){
        if (_coord_buffer == null) {
            _coord_buffer = ByteBuffer.allocateDirect(coordVerticesFVH.length * 4);
            _coord_buffer.order(ByteOrder.nativeOrder());
            _coord_buffer.asFloatBuffer().put(coordVerticesFVH);
            _coord_buffer.position(0);
        }else{
            _coord_buffer.clear();
            _coord_buffer.asFloatBuffer().put(coordVerticesFVH);
            _coord_buffer.position(0);
        }
    }

    static float[] squareVertices = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, }; // fullscreen

    static float[] squareVertices1 = { -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, }; // left-top

    static float[] squareVertices2 = { 0.0f, -1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, }; // right-bottom

    static float[] squareVertices3 = { -1.0f, -1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, }; // left-bottom

    static float[] squareVertices4 = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, }; // right-top

    private static float[] coordVertices =
            { 0.0f, 1.0f,
              1.0f, 1.0f,
              0.0f, 0.0f,
              1.0f, 0.0f, };// whole-texture
    private static float[] coorVerticesFV =
            { 1.0f, 1.0f,
              0.0f, 1.0f,
              1.0f, 0.0f,
              0.0f, 0.0f, };// whole-texture
    private static float[] coorVerticesFH =
            { 0.0f, 0.0f,
              1.0f, 0.0f,
              0.0f, 1.0f,
              1.0f, 1.0f, };// whole-texture
    private static float[] coordVerticesFVH =
            { 1.0f, 0.0f,
              0.0f, 0.0f,
              1.0f, 1.0f,
              0.0f, 1.0f, };// whole-texture

    private static final String VERTEX_SHADER = "attribute vec4 vPosition;\n" + "attribute vec2 a_texCoord;\n"
            + "varying vec2 tc;\n" + "void main() {\n" + "gl_Position = vPosition;\n" + "tc = a_texCoord;\n" + "}\n";

    private static final String FRAGMENT_SHADER =
            "precision mediump float;\n"
            + "uniform int orderVU;" // 0==UV, 1==VU
            + "uniform int type;" // 0==planar, 1=semiplanar

            + "vec4 planarYUV(sampler2D plane0, sampler2D plane1, vec2 coordinate){" +
                    " float Y = texture2D(plane0, coordinate).r;" +
                    " float U = texture2D(plane1, coordinate * vec2(1.0, 0.5)).r;" +
                    " float V = texture2D(plane1, coordinate * vec2(1.0, 0.5) + vec2(0.0, 0.5)).r;" +
                    " return vec4(Y, U, V, 1.0);" +
                    "}" +

                    "vec4 semiplanarYUV(sampler2D plane0, sampler2D plane1, highp vec2 coordinate){" +
                    " float Y = texture2D(plane0, coordinate).r;" +
                    " vec4 UV = texture2D(plane1, coordinate);" +
                    " return vec4(Y, UV.r, UV.a, 1.0);" +
                    "}"

            + "vec4 YUV(sampler2D plane0, sampler2D plane1, vec2 coordinate){" +
            " vec4 yuv;" +
            " if(type == 0) yuv = planarYUV(plane0, plane1, coordinate);" +
            " if(type == 1) yuv = semiplanarYUV(plane0, plane1, coordinate);" +
            " if(orderVU == 0) return yuv;" +
            " return vec4(yuv.x, yuv.z, yuv.y, yuv.w);" +
            "}"

            + "uniform sampler2D tex_y;\n"
            + "uniform sampler2D tex_uv;\n"
            + "varying vec2 tc;\n"
            + "void main() {\n"
            + "     vec4 yuv = YUV(tex_y, tex_uv, tc);"
            + "     vec4 c = vec4((yuv.x - 16./255.) * 1.164);\n"
            + "     vec4 U = vec4(yuv.y - 128./255.);\n"
            + "     vec4 V = vec4(yuv.z - 128./255.);\n"
            + "     c += V * vec4(1.596, -0.813, 0, 0);\n"
            + "     c += U * vec4(0, -0.392, 2.017, 0);\n"
            + "     c.a = 1.0;\n"
            + "     gl_FragColor = c;\n"
            + "}\n";
}