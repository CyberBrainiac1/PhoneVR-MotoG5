// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// rendering/VRRenderer.kt — OpenGL ES 2.0 stereo renderer with barrel distortion.
// Renders the decoded video texture (OES) to the phone display with
// side-by-side stereo and barrel distortion compensation for cardboard viewers.

package com.phonevrmotog5.rendering

import android.content.Context
import android.graphics.SurfaceTexture
import android.opengl.GLES11Ext
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class VRRenderer(private val context: Context) : GLSurfaceView.Renderer {

    companion object {
        private const val TAG = "PhoneVR-Render"

        // Barrel distortion coefficients for a standard cardboard viewer.
        // Tune these for your headset lens geometry.
        private const val K1 = 0.22f
        private const val K2 = 0.24f

        // Full-screen quad vertices: x, y, u, v
        private val QUAD_VERTS = floatArrayOf(
            -1f, -1f, 0f, 0f,
             1f, -1f, 1f, 0f,
            -1f,  1f, 0f, 1f,
             1f,  1f, 1f, 1f
        )

        private val VERTEX_SHADER = """
            attribute vec2 a_position;
            attribute vec2 a_texCoord;
            varying   vec2 v_texCoord;
            void main() {
                gl_Position = vec4(a_position, 0.0, 1.0);
                v_texCoord  = a_texCoord;
            }
        """.trimIndent()

        // Fragment shader for one eye. Applies barrel distortion around the
        // eye centre (0.5, 0.5 in eye UV space) and samples the OES video texture.
        private val FRAGMENT_SHADER = """
            #extension GL_OES_EGL_image_external : require
            precision mediump float;
            uniform samplerExternalOES u_texture;
            uniform float u_k1;
            uniform float u_k2;
            uniform float u_xOffset;  // 0.0 left eye, 0.5 right eye (in full-frame UV)
            varying vec2 v_texCoord;
            void main() {
                // Map eye UV [0,1]×[0,1] → undistorted UV in the half-frame
                vec2 eyeUV = v_texCoord;
                vec2 centre = vec2(0.5, 0.5);
                vec2 d = eyeUV - centre;
                float r2 = dot(d, d);
                float distort = 1.0 + u_k1 * r2 + u_k2 * r2 * r2;
                vec2 distortedUV = centre + d * distort;

                // Remap to half-frame in the side-by-side texture
                vec2 texUV = vec2(u_xOffset + distortedUV.x * 0.5, distortedUV.y);

                // Clamp to valid range to avoid border artefacts
                texUV = clamp(texUV, vec2(u_xOffset, 0.0), vec2(u_xOffset + 0.5, 1.0));
                gl_FragColor = texture2D(u_texture, texUV);
            }
        """.trimIndent()
    }

    private var program       = 0
    private var textureId     = 0
    private var surfaceTexture: SurfaceTexture? = null
    private var outputSurface:  Surface?         = null

    @Volatile private var currentQuat = floatArrayOf(0f, 0f, 0f, 1f)

    private lateinit var quadBuffer: FloatBuffer

    fun getVideoSurface(): Surface? = outputSurface

    fun updatePose(quat: FloatArray) {
        currentQuat = quat.copyOf()
    }

    // ── GLSurfaceView.Renderer ────────────────────────────────────────────────

    override fun onSurfaceCreated(gl: GL10, config: EGLConfig) {
        GLES20.glClearColor(0f, 0f, 0f, 1f)

        program   = buildProgram(VERTEX_SHADER, FRAGMENT_SHADER)
        textureId = createOesTexture()

        surfaceTexture = SurfaceTexture(textureId)
        outputSurface  = Surface(surfaceTexture)

        // Upload quad vertices
        quadBuffer = ByteBuffer.allocateDirect(QUAD_VERTS.size * 4)
            .order(ByteOrder.nativeOrder())
            .asFloatBuffer()
            .also { it.put(QUAD_VERTS); it.position(0) }

        Log.i(TAG, "GL surface created, OES texture=$textureId")
    }

    override fun onSurfaceChanged(gl: GL10, width: Int, height: Int) {
        GLES20.glViewport(0, 0, width, height)
    }

    override fun onDrawFrame(gl: GL10) {
        surfaceTexture?.updateTexImage()

        GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT)
        GLES20.glUseProgram(program)

        val posAttr  = GLES20.glGetAttribLocation(program, "a_position")
        val texAttr  = GLES20.glGetAttribLocation(program, "a_texCoord")
        val texUnif  = GLES20.glGetUniformLocation(program, "u_texture")
        val k1Unif   = GLES20.glGetUniformLocation(program, "u_k1")
        val k2Unif   = GLES20.glGetUniformLocation(program, "u_k2")
        val offUnif  = GLES20.glGetUniformLocation(program, "u_xOffset")

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0)
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureId)
        GLES20.glUniform1i(texUnif, 0)
        GLES20.glUniform1f(k1Unif, K1)
        GLES20.glUniform1f(k2Unif, K2)

        // Stride: 4 floats per vertex (x,y,u,v), 4 bytes each
        val stride = 4 * 4
        quadBuffer.position(0)
        GLES20.glVertexAttribPointer(posAttr, 2, GLES20.GL_FLOAT, false, stride, quadBuffer)
        GLES20.glEnableVertexAttribArray(posAttr)

        quadBuffer.position(2)
        GLES20.glVertexAttribPointer(texAttr, 2, GLES20.GL_FLOAT, false, stride, quadBuffer)
        GLES20.glEnableVertexAttribArray(texAttr)

        // Left eye: x in [-1,0], xOffset=0.0
        GLES20.glViewport(0, 0, /* will be set per eye in onSurfaceChanged */ 540, 540)
        GLES20.glUniform1f(offUnif, 0.0f)
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4)

        // Right eye: x in [0,1], xOffset=0.5
        GLES20.glViewport(540, 0, 540, 540)
        GLES20.glUniform1f(offUnif, 0.5f)
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4)
    }

    // ── GL helpers ────────────────────────────────────────────────────────────

    private fun createOesTexture(): Int {
        val ids = IntArray(1)
        GLES20.glGenTextures(1, ids, 0)
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, ids[0])
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR)
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES,
            GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR)
        return ids[0]
    }

    private fun compileShader(type: Int, src: String): Int {
        val shader = GLES20.glCreateShader(type)
        GLES20.glShaderSource(shader, src)
        GLES20.glCompileShader(shader)
        val status = IntArray(1)
        GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, status, 0)
        if (status[0] == 0) {
            Log.e(TAG, "Shader compile error: ${GLES20.glGetShaderInfoLog(shader)}")
            GLES20.glDeleteShader(shader)
            return 0
        }
        return shader
    }

    private fun buildProgram(vertSrc: String, fragSrc: String): Int {
        val vert = compileShader(GLES20.GL_VERTEX_SHADER,   vertSrc)
        val frag = compileShader(GLES20.GL_FRAGMENT_SHADER, fragSrc)
        val prog = GLES20.glCreateProgram()
        GLES20.glAttachShader(prog, vert)
        GLES20.glAttachShader(prog, frag)
        GLES20.glLinkProgram(prog)
        return prog
    }
}
