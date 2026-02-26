// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/ConnectionManager.kt — TCP control channel + UDP pose sender.
// Manages connection to the PC driver, receives the video stream header,
// and sends pose data at sensor rate.

package com.phonevrmotog5.network

import android.util.Log
import android.view.Surface
import com.phonevrmotog5.video.VideoDecoder
import java.io.DataInputStream
import java.io.IOException
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.Socket
import java.nio.ByteBuffer
import java.nio.ByteOrder

class ConnectionManager(
    private val pcIp: String,
    private val videoSurface: Surface?
) {
    companion object {
        private const val TAG          = "PhoneVR-Connection"
        private const val CONTROL_PORT = 33334
        private const val POSE_PORT    = 33335

        // VideoFrameHeader field sizes (matches PC driver struct, 40 bytes total)
        private const val HEADER_SIZE  = 40
    }

    private var controlSocket:  Socket?       = null
    private var poseSocket:     DatagramSocket? = null
    private var videoDecoder:   VideoDecoder? = null
    private var receiveThread:  Thread?       = null

    @Volatile private var running = false

    /**
     * Connect to the PC driver on the control/video port and start receiving
     * video frames. Blocks the calling coroutine until disconnected.
     */
    fun connect() {
        running = true
        Log.i(TAG, "Connecting to $pcIp:$CONTROL_PORT …")

        try {
            controlSocket = Socket(pcIp, CONTROL_PORT)
            poseSocket    = DatagramSocket()
            videoDecoder  = VideoDecoder(videoSurface)
            videoDecoder?.start()

            Log.i(TAG, "Connected — starting receive loop")
            receiveLoop(DataInputStream(controlSocket!!.getInputStream()))
        } catch (e: IOException) {
            Log.e(TAG, "Connection error: ${e.message}")
        } finally {
            disconnect()
        }
    }

    /** Disconnect cleanly. */
    fun disconnect() {
        running = false
        videoDecoder?.stop()
        try { controlSocket?.close() } catch (_: IOException) {}
        poseSocket?.close()
        controlSocket = null
        poseSocket    = null
        Log.i(TAG, "Disconnected")
    }

    /**
     * Send a pose UDP packet to the PC driver.
     * Format: float[4] quaternion (x,y,z,w) + float[3] accelerometer = 28 bytes
     */
    fun sendPose(quat: FloatArray, accel: FloatArray) {
        val buf = ByteBuffer.allocate(28).order(ByteOrder.LITTLE_ENDIAN)
        quat.forEach  { buf.putFloat(it) }
        accel.forEach { buf.putFloat(it) }
        val data    = buf.array()
        val pcAddr  = InetAddress.getByName(pcIp)
        val packet  = DatagramPacket(data, data.size, pcAddr, POSE_PORT)
        try {
            poseSocket?.send(packet)
        } catch (e: IOException) {
            Log.w(TAG, "sendPose error: ${e.message}")
        }
    }

    // ── Private ───────────────────────────────────────────────────────────────

    private fun receiveLoop(input: DataInputStream) {
        val headerBuf = ByteArray(HEADER_SIZE)

        while (running) {
            try {
                // Read VideoFrameHeader (40 bytes)
                input.readFully(headerBuf)
                val header = ByteBuffer.wrap(headerBuf).order(ByteOrder.LITTLE_ENDIAN)
                val pts       = header.getLong()    // presentation timestamp µs
                // quaternion at capture: x,y,z,w (skip — used for timewarp on PC)
                header.getFloat(); header.getFloat(); header.getFloat(); header.getFloat()
                val frameSize = header.getInt()
                // fps, capture_time (informational)

                if (frameSize <= 0 || frameSize > 4 * 1024 * 1024) {
                    Log.w(TAG, "Suspicious frame_size=$frameSize — skipping")
                    continue
                }

                // Read H.264 NAL data
                val nalData = ByteArray(frameSize)
                input.readFully(nalData)
                videoDecoder?.decodeFrame(nalData, pts)

            } catch (e: IOException) {
                if (running) Log.e(TAG, "receiveLoop IO error: ${e.message}")
                break
            }
        }
    }
}
