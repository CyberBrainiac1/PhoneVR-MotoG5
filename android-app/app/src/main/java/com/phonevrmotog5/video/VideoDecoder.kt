// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// video/VideoDecoder.kt — MediaCodec H.264 decoder.
// Uses only APIs available on Android 7.0 (API 24+).
// No ALVR client core — no AImageReader_newWithUsage (API 26).

package com.phonevrmotog5.video

import android.media.MediaCodec
import android.media.MediaFormat
import android.util.Log
import android.view.Surface

class VideoDecoder(private val surface: Surface?) {

    companion object {
        private const val TAG       = "PhoneVR-Video"
        private const val MIME_TYPE = "video/avc" // H.264
        private const val TIMEOUT_US = 10_000L    // 10 ms dequeue timeout
    }

    private var codec: MediaCodec? = null

    fun start() {
        if (surface == null) {
            Log.w(TAG, "No output surface provided — decoder will not start")
            return
        }

        try {
            codec = MediaCodec.createDecoderByType(MIME_TYPE)

            val format = MediaFormat.createVideoFormat(MIME_TYPE, 1920, 540)
            // Low latency configuration — API 24+
            format.setInteger(MediaFormat.KEY_PRIORITY, 0)              // real-time
            // KEY_LOW_LATENCY is API 30+; on API 24-29 the decoder may still
            // honour the absence of B-frames due to Baseline profile.

            codec?.configure(format, surface, null, 0)
            codec?.start()
            Log.i(TAG, "VideoDecoder started")
        } catch (e: Exception) {
            Log.e(TAG, "Decoder start error: ${e.message}")
        }
    }

    /**
     * Queue a raw H.264 NAL unit for decoding. Called from the receive loop thread.
     * @param data    Annex-B H.264 NAL data (may contain SPS/PPS + slice)
     * @param ptsUs   Presentation timestamp in microseconds
     */
    fun decodeFrame(data: ByteArray, ptsUs: Long) {
        val c = codec ?: return
        try {
            val inputIndex = c.dequeueInputBuffer(TIMEOUT_US)
            if (inputIndex < 0) {
                Log.d(TAG, "No input buffer available — dropping frame")
                return
            }

            val inputBuffer = c.getInputBuffer(inputIndex) ?: return
            inputBuffer.clear()
            inputBuffer.put(data)
            c.queueInputBuffer(inputIndex, 0, data.size, ptsUs, 0)

            // Release any decoded output frames to the Surface
            val info = MediaCodec.BufferInfo()
            var outputIndex = c.dequeueOutputBuffer(info, 0)
            while (outputIndex >= 0) {
                // render = true: the buffer is released to the Surface for display
                c.releaseOutputBuffer(outputIndex, true)
                outputIndex = c.dequeueOutputBuffer(info, 0)
            }
        } catch (e: Exception) {
            Log.e(TAG, "decodeFrame error: ${e.message}")
        }
    }

    fun stop() {
        try {
            codec?.stop()
            codec?.release()
        } catch (e: Exception) {
            Log.w(TAG, "Decoder stop error: ${e.message}")
        }
        codec = null
        Log.i(TAG, "VideoDecoder stopped")
    }
}
