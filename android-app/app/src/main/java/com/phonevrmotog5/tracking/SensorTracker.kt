// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// tracking/SensorTracker.kt — Sensor listener that feeds into PoseEstimator.
// Prefers gyroscope (TYPE_GYROSCOPE). If not available (only the budget Moto G5 Play
// XT1920 lacks one — the standard Moto G5 XT1675/XT1676 and G5 Plus XT1686/XT1687
// both include a gyroscope) falls back to accelerometer + magnetometer with Madgwick filter.

package com.phonevrmotog5.tracking

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.util.Log

/**
 * @param onPose Callback invoked with (quaternion float[4], accel float[3])
 *               on every sensor update.
 */
class SensorTracker(
    context: Context,
    private val onPose: (quat: FloatArray, accel: FloatArray) -> Unit
) : SensorEventListener {

    companion object {
        private const val TAG = "PhoneVR-Sensors"
    }

    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val estimator     = PoseEstimator()

    private val gyro       = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
    private val accel      = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
    private val magnetic   = sensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD)

    val hasGyroscope: Boolean = (gyro != null)

    init {
        if (!hasGyroscope) {
            Log.w(TAG, "No gyroscope found — using accelerometer + magnetometer fallback. " +
                       "Tracking quality will be reduced.")
        } else {
            Log.i(TAG, "Gyroscope available")
        }
    }

    fun register() {
        if (hasGyroscope) {
            sensorManager.registerListener(this, gyro,    SensorManager.SENSOR_DELAY_FASTEST)
            sensorManager.registerListener(this, accel,   SensorManager.SENSOR_DELAY_FASTEST)
        } else {
            // Magnetometer-based fallback
            sensorManager.registerListener(this, accel,   SensorManager.SENSOR_DELAY_FASTEST)
            sensorManager.registerListener(this, magnetic, SensorManager.SENSOR_DELAY_FASTEST)
        }
    }

    fun unregister() {
        sensorManager.unregisterListener(this)
    }

    /** Reset yaw to "current forward direction". */
    fun recenter() {
        estimator.recenter()
    }

    override fun onSensorChanged(event: SensorEvent) {
        when (event.sensor.type) {
            Sensor.TYPE_GYROSCOPE      -> estimator.onGyro(event.values, event.timestamp)
            Sensor.TYPE_ACCELEROMETER  -> estimator.onAccel(event.values, event.timestamp)
            Sensor.TYPE_MAGNETIC_FIELD -> estimator.onMagnetic(event.values)
        }
        val quat  = estimator.getQuaternion()
        val acc   = estimator.getLastAccel()
        onPose(quat, acc)
    }

    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {
        Log.d(TAG, "Sensor accuracy changed: ${sensor.name} → $accuracy")
    }
}
