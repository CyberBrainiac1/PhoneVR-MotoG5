// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// service/StreamingService.kt — Foreground service that owns the streaming
// connection lifecycle and holds WAKE_LOCK + WIFI_LOCK to prevent Android
// from suspending the network stack during VR sessions.

package com.phonevrmotog5.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.net.wifi.WifiManager
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.util.Log
import android.view.Surface
import androidx.core.app.NotificationCompat
import com.phonevrmotog5.R
import com.phonevrmotog5.network.ConnectionManager
import kotlinx.coroutines.*

class StreamingService : Service() {

    companion object {
        private const val TAG              = "PhoneVR-MotoG5"
        private const val CHANNEL_ID      = "phonevr_streaming"
        private const val NOTIFICATION_ID = 1
    }

    inner class LocalBinder : Binder() {
        fun getService(): StreamingService = this@StreamingService
    }

    private val binder = LocalBinder()
    private val scope  = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private var wakeLock:  PowerManager.WakeLock?      = null
    private var wifiLock:  WifiManager.WifiLock?       = null
    private var connMgr:   ConnectionManager?           = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification("Connecting…"))
        acquireLocks()
        Log.i(TAG, "StreamingService created")
    }

    override fun onBind(intent: Intent): IBinder = binder

    fun startStreaming(pcIp: String, videoSurface: Surface?) {
        Log.i(TAG, "startStreaming to $pcIp")
        connMgr = ConnectionManager(pcIp, videoSurface)
        scope.launch {
            connMgr?.connect()
        }
        updateNotification("Streaming to $pcIp")
    }

    fun stopStreaming() {
        Log.i(TAG, "stopStreaming")
        connMgr?.disconnect()
        connMgr = null
        updateNotification("Idle")
    }

    fun sendPose(quat: FloatArray, accel: FloatArray) {
        scope.launch { connMgr?.sendPose(quat, accel) }
    }

    override fun onDestroy() {
        scope.cancel()
        connMgr?.disconnect()
        releaseLocks()
        Log.i(TAG, "StreamingService destroyed")
        super.onDestroy()
    }

    // ── Locks ─────────────────────────────────────────────────────────────────

    private fun acquireLocks() {
        val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = pm.newWakeLock(
            PowerManager.PARTIAL_WAKE_LOCK,
            "PhoneVR:StreamingWakeLock")
        wakeLock?.acquire(10 * 60 * 1000L) // max 10 min; re-acquired on reconnect

        val wm = applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        wifiLock = wm.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "PhoneVR:WifiLock")
        wifiLock?.acquire()
    }

    private fun releaseLocks() {
        if (wakeLock?.isHeld == true) wakeLock?.release()
        if (wifiLock?.isHeld == true) wifiLock?.release()
    }

    // ── Notification ──────────────────────────────────────────────────────────

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "PhoneVR Streaming",
                NotificationManager.IMPORTANCE_LOW
            )
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(status: String): Notification =
        NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(status)
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setOngoing(true)
            .build()

    private fun updateNotification(status: String) {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.notify(NOTIFICATION_ID, buildNotification(status))
    }
}
