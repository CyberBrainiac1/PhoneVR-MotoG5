// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// tracking/PoseEstimator.kt — Sensor fusion for head tracking.
//
// Two modes:
//  1. Gyroscope mode: integrate gyro rates with complementary filter using
//     accelerometer for gravity correction.
//  2. Accel+Mag mode: Madgwick AHRS filter (no gyro).
//
// References:
//   - Madgwick, S.O.H. (2010). "An efficient orientation filter for inertial
//     and inertial/magnetic sensor arrays."

package com.phonevrmotog5.tracking

import kotlin.math.sqrt

class PoseEstimator {

    // Current orientation quaternion (x, y, z, w)
    private val quat    = floatArrayOf(0f, 0f, 0f, 1f)
    private var recenterQuat = floatArrayOf(0f, 0f, 0f, 1f)

    private var lastAccel   = floatArrayOf(0f, 0f, 0f)
    private var lastMag     = floatArrayOf(0f, 0f, 0f)
    private var lastGyroTs  = 0L  // nanoseconds

    // Madgwick filter gain — higher = faster response but more noise
    private val beta = 0.1f

    // Complementary filter coefficient for gyro mode
    private val alpha = 0.98f

    fun onGyro(values: FloatArray, timestampNs: Long) {
        if (lastGyroTs == 0L) { lastGyroTs = timestampNs; return }

        val dt = (timestampNs - lastGyroTs) / 1_000_000_000f
        lastGyroTs = timestampNs
        if (dt <= 0f || dt > 0.1f) return // sanity check

        // Integrate angular velocity to quaternion
        val wx = values[0]; val wy = values[1]; val wz = values[2]
        val halfDt = dt * 0.5f

        val qx = quat[0]; val qy = quat[1]; val qz = quat[2]; val qw = quat[3]
        quat[0] = qx + halfDt * ( qw*wx - qz*wy + qy*wz)
        quat[1] = qy + halfDt * ( qz*wx + qw*wy - qx*wz)
        quat[2] = qz + halfDt * (-qy*wx + qx*wy + qw*wz)
        quat[3] = qw + halfDt * (-qx*wx - qy*wy - qz*wz)
        normalizeQuat()

        // Gravity correction using accelerometer (complementary filter)
        fusionWithAccel()
    }

    fun onAccel(values: FloatArray, @Suppress("UNUSED_PARAMETER") timestampNs: Long) {
        lastAccel[0] = values[0]
        lastAccel[1] = values[1]
        lastAccel[2] = values[2]

        // In Madgwick mode (no gyro), run filter on every accel update
        if (lastGyroTs == 0L) {
            madgwickUpdate(null, lastAccel, lastMag)
        }
    }

    fun onMagnetic(values: FloatArray) {
        lastMag[0] = values[0]
        lastMag[1] = values[1]
        lastMag[2] = values[2]
    }

    fun getQuaternion(): FloatArray {
        // Apply recenter offset: q_recentered = recenterQuat^-1 * quat
        val inv  = quatInverse(recenterQuat)
        return quatMultiply(inv, quat)
    }

    fun getLastAccel(): FloatArray = lastAccel.copyOf()

    fun recenter() {
        recenterQuat = quat.copyOf()
    }

    // ── Complementary filter for gyro mode ───────────────────────────────────

    private fun fusionWithAccel() {
        val ax = lastAccel[0]; val ay = lastAccel[1]; val az = lastAccel[2]
        val mag = sqrt(ax*ax + ay*ay + az*az)
        if (mag < 1e-6f) return

        // Convert accel to orientation quaternion (gravity reference)
        val roll  = Math.atan2(ay.toDouble(), az.toDouble()).toFloat()
        val pitch = Math.atan2((-ax).toDouble(), sqrt((ay*ay + az*az).toDouble())).toFloat()

        // Build accel quaternion from roll/pitch only (no yaw correction from accel)
        val halfRoll  = roll  / 2f
        val halfPitch = pitch / 2f
        val accelQ = floatArrayOf(
            Math.sin(halfPitch.toDouble()).toFloat() * Math.cos(halfRoll.toDouble()).toFloat(),
            Math.cos(halfPitch.toDouble()).toFloat() * Math.sin(halfRoll.toDouble()).toFloat(),
           -Math.sin(halfPitch.toDouble()).toFloat() * Math.sin(halfRoll.toDouble()).toFloat(),
            Math.cos(halfPitch.toDouble()).toFloat() * Math.cos(halfRoll.toDouble()).toFloat()
        )

        // Blend: q = alpha * q_gyro + (1 - alpha) * q_accel
        for (i in 0..3) {
            quat[i] = alpha * quat[i] + (1f - alpha) * accelQ[i]
        }
        normalizeQuat()
    }

    // ── Madgwick AHRS (accel + mag, no gyro) ─────────────────────────────────

    private fun madgwickUpdate(gyro: FloatArray?, accel: FloatArray, mag: FloatArray) {
        // Simplified Madgwick step without gyro (gradient descent only).
        // Reference implementation: x-io Technologies MadgwickAHRS.
        val ax = accel[0]; val ay = accel[1]; val az = accel[2]
        val mx = mag[0];   val my = mag[1];   val mz = mag[2]

        val aMag = sqrt(ax*ax + ay*ay + az*az)
        if (aMag < 1e-6f) return

        val ax_n = ax / aMag; val ay_n = ay / aMag; val az_n = az / aMag

        val qx = quat[0]; val qy = quat[1]; val qz = quat[2]; val qw = quat[3]

        // Gradient from accelerometer objective function (gravity alignment)
        val f1 = 2f * (qx*qz - qw*qy) - ax_n
        val f2 = 2f * (qw*qx + qy*qz) - ay_n
        val f3 = 1f - 2f * (qx*qx + qy*qy) - az_n

        val j11 = -2f*qy; val j12 =  2f*qz; val j13 = -2f*qw; val j14 =  2f*qx
        val j21 =  2f*qx; val j22 =  2f*qw; val j23 =  2f*qz; val j24 =  2f*qy
        val j31 =  0f;    val j32 = -4f*qx; val j33 = -4f*qy; val j34 =  0f

        val gx = j11*f1 + j21*f2 + j31*f3
        val gy = j12*f1 + j22*f2 + j32*f3
        val gz = j13*f1 + j23*f2 + j33*f3
        val gw = j14*f1 + j24*f2 + j34*f3

        val gMag = sqrt(gx*gx + gy*gy + gz*gz + gw*gw)
        if (gMag < 1e-6f) return

        // Step size proportional to beta
        val step = beta / gMag
        quat[0] -= step * gx
        quat[1] -= step * gy
        quat[2] -= step * gz
        quat[3] -= step * gw
        normalizeQuat()
    }

    // ── Quaternion utilities ──────────────────────────────────────────────────

    private fun normalizeQuat() {
        val mag = sqrt(quat[0]*quat[0] + quat[1]*quat[1] +
                       quat[2]*quat[2] + quat[3]*quat[3])
        if (mag > 1e-6f) {
            quat[0] /= mag; quat[1] /= mag; quat[2] /= mag; quat[3] /= mag
        }
    }

    private fun quatInverse(q: FloatArray): FloatArray =
        floatArrayOf(-q[0], -q[1], -q[2], q[3])

    private fun quatMultiply(a: FloatArray, b: FloatArray): FloatArray {
        // Hamilton product
        return floatArrayOf(
             a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
             a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
             a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
             a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]
        )
    }
}
