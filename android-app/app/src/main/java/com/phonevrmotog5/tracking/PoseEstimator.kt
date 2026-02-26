// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// tracking/PoseEstimator.kt — Head pose from Android's built-in sensor fusion.
//
// Android's TYPE_ROTATION_VECTOR sensor runs a hardware-assisted EKF that fuses
// gyroscope + accelerometer + magnetometer.  Calling SensorManager.getQuaternionFromVector()
// gives a normalised quaternion directly — no DIY Madgwick or complementary filter needed.
//
// Reference implementations consulted:
//   - vlad-abobus/vrYoy SensorHelper.kt — TYPE_ROTATION_VECTOR pattern in Kotlin
//   - xioTechnologies/Fusion FusionAhrs.c — canonical Madgwick reference (gain=0.5,
//     acceleration rejection, 3-second initialisation ramp) — confirms why a custom
//     implementation is fragile and why the OS sensor is preferable.
//   - Android developer docs: SensorManager.getQuaternionFromVector (API 18)

package com.phonevrmotog5.tracking

class PoseEstimator {

    // Current orientation quaternion — internal storage is (x, y, z, w)
    private val quat         = floatArrayOf(0f, 0f, 0f, 1f)
    // Quaternion captured at the last recenter() call
    private var recenterQuat = floatArrayOf(0f, 0f, 0f, 1f)

    // Latest raw accelerometer values — forwarded to ConnectionManager for pose packets
    private var lastAccel    = floatArrayOf(0f, 0f, 0f)

    // ── Called by SensorTracker ───────────────────────────────────────────────

    /**
     * Set the orientation from Android's rotation-vector sensor.
     *
     * SensorManager.getQuaternionFromVector() returns (w, x, y, z).
     * We store internally as (x, y, z, w) to match the wire protocol used by
     * ConnectionManager.sendPose().
     *
     * @param androidQuat float[4] in Android's (w, x, y, z) order, already
     *                    normalised by the OS.
     */
    fun setQuaternion(androidQuat: FloatArray) {
        quat[0] = androidQuat[1] // x
        quat[1] = androidQuat[2] // y
        quat[2] = androidQuat[3] // z
        quat[3] = androidQuat[0] // w
    }

    /** Store the latest raw accelerometer reading (m/s²). */
    fun onAccel(values: FloatArray) {
        lastAccel[0] = values[0]
        lastAccel[1] = values[1]
        lastAccel[2] = values[2]
    }

    // ── Output ────────────────────────────────────────────────────────────────

    /**
     * Returns the recentered orientation quaternion (x, y, z, w).
     * q_out = recenterQuat⁻¹ ⊗ quat
     */
    fun getQuaternion(): FloatArray {
        val inv = quatInverse(recenterQuat)
        return quatMultiply(inv, quat)
    }

    fun getLastAccel(): FloatArray = lastAccel.copyOf()

    /** Capture current heading as the "forward" reference for future getQuaternion() calls. */
    fun recenter() {
        recenterQuat = quat.copyOf()
    }

    // ── Quaternion utilities ──────────────────────────────────────────────────

    private fun quatInverse(q: FloatArray): FloatArray =
        floatArrayOf(-q[0], -q[1], -q[2], q[3])

    private fun quatMultiply(a: FloatArray, b: FloatArray): FloatArray {
        // Hamilton product: a ⊗ b, both in (x, y, z, w) order
        return floatArrayOf(
             a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
             a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
             a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
             a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]
        )
    }
}
