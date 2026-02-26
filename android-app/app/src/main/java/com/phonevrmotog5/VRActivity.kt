// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// VRActivity.kt — VR rendering activity.
// Manages the GLSurfaceView, sensor tracking, video decoding, and streaming
// service lifecycle. Displayed in landscape full-screen mode.

package com.phonevrmotog5

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.os.IBinder
import android.view.GestureDetector
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.phonevrmotog5.rendering.VRRenderer
import com.phonevrmotog5.service.StreamingService
import com.phonevrmotog5.tracking.SensorTracker

class VRActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_PC_IP = "pc_ip"
    }

    private lateinit var glSurfaceView: GLSurfaceView
    private lateinit var renderer:      VRRenderer
    private lateinit var sensorTracker: SensorTracker
    private lateinit var tvStats:       TextView
    private lateinit var btnRecenter:   Button

    private var streamingService: StreamingService? = null
    private var serviceBound = false
    private lateinit var pcIp: String

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            val b = binder as StreamingService.LocalBinder
            streamingService = b.getService()
            serviceBound = true
            streamingService?.startStreaming(pcIp, renderer.getVideoSurface())
        }
        override fun onServiceDisconnected(name: ComponentName) {
            streamingService = null
            serviceBound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_vr)

        // Full-screen immersive
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_FULLSCREEN
            or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
            or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
        )

        pcIp = intent.getStringExtra(EXTRA_PC_IP) ?: "192.168.1.100"

        glSurfaceView = findViewById(R.id.gl_surface_view)
        tvStats       = findViewById(R.id.tv_stats)
        btnRecenter   = findViewById(R.id.btn_recenter)

        // OpenGL ES 2.0 renderer
        glSurfaceView.setEGLContextClientVersion(2)
        renderer = VRRenderer(this)
        glSurfaceView.setRenderer(renderer)
        glSurfaceView.renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY

        // Sensor tracker (gyro preferred, accel+mag fallback)
        sensorTracker = SensorTracker(this) { quat, _ ->
            renderer.updatePose(quat)
        }

        // Double-tap to recenter
        val gestureDetector = GestureDetector(this,
            object : GestureDetector.SimpleOnGestureListener() {
                override fun onDoubleTap(e: MotionEvent): Boolean {
                    sensorTracker.recenter()
                    return true
                }
            })
        glSurfaceView.setOnTouchListener { _, e -> gestureDetector.onTouchEvent(e); true }

        btnRecenter.setOnClickListener { sensorTracker.recenter() }

        // Bind streaming service
        val intent = Intent(this, StreamingService::class.java)
        bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
    }

    override fun onResume() {
        super.onResume()
        glSurfaceView.onResume()
        sensorTracker.register()
    }

    override fun onPause() {
        super.onPause()
        glSurfaceView.onPause()
        sensorTracker.unregister()
    }

    override fun onDestroy() {
        if (serviceBound) {
            streamingService?.stopStreaming()
            unbindService(serviceConnection)
            serviceBound = false
        }
        super.onDestroy()
    }
}
