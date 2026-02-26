// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// tracking/SensorTracker.kt — Android sensor listener feeding PoseEstimator.
//
// Uses Android's built-in TYPE_ROTATION_VECTOR sensor (hardware EKF that fuses
// gyroscope + accelerometer + magnetometer at the OS level) instead of a custom
// Madgwick/complementary filter.  This approach is used by Google Cardboard, vrYoy,
// and most production Android VR apps — it is far more battle-tested than DIY math.
//
// Device behaviour:
//   Moto G5 (XT1675/XT1676), G5 Plus (XT1686/XT1687):
//       TYPE_ROTATION_VECTOR fuses gyroscope + accel + mag → full quality.
//   Moto G5 Play (XT1920, no gyroscope):
//       TYPE_ROTATION_VECTOR falls back internally to TYPE_GEOMAGNETIC_ROTATION_VECTOR
//       (accel + mag only); we explicitly register the geomagnetic sensor as backup.
//
// References:
//   - vlad-abobus/vrYoy SensorHelper.kt — TYPE_ROTATION_VECTOR + getQuaternionFromVector
//   - Android docs: SensorManager.getQuaternionFromVector (API 18, stable)

package com.phonevrmotog5.tracking

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.util.Log

/**
 * @param onPose Callback invoked with (quaternion float[4] in x,y,z,w, accel float[3])
 *               on every rotation-vector update.
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

    // Primary: Android hardware EKF (gyro+accel+mag when gyro present, else accel+mag)
    private val rotVecSensor     = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)

    // Explicit fallback for devices that do not report TYPE_ROTATION_VECTOR (rare on API 24+)
    private val geoRotVecSensor  = sensorManager.getDefaultSensor(
        Sensor.TYPE_GEOMAGNETIC_ROTATION_VECTOR)

    // Raw accelerometer — forwarded as-is in pose packets so the PC can do timewarp
    private val accelSensor      = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)

    // Gyroscope detection is kept only to drive the "no gyro" warning banner in the UI.
    // The Moto G5 (XT1675/XT1676) and G5 Plus both include a gyroscope.
    // Only the budget Moto G5 Play (XT1920) does not.
    val hasGyroscope: Boolean    = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE) != null

    init {
        if (!hasGyroscope) {
            Log.w(TAG, "No gyroscope — TYPE_ROTATION_VECTOR will use accel+mag fusion. " +
                       "This is expected on the Moto G5 Play (XT1920). " +
                       "Standard G5 (XT1675/XT1676) and G5 Plus include a gyroscope.")
        } else {
            Log.i(TAG, "Gyroscope available — full hardware EKF sensor fusion active")
        }
    }

    fun register() {
        // SENSOR_DELAY_FASTEST: minimise motion-to-photon latency for VR (~200 Hz on Snapdragon)
        val primary = rotVecSensor ?: geoRotVecSensor
        if (primary != null) {
            sensorManager.registerListener(this, primary, SensorManager.SENSOR_DELAY_FASTEST)
            Log.d(TAG, "Registered ${primary.name}")
        } else {
            Log.e(TAG, "No rotation-vector sensor found — pose will not update")
        }
        accelSensor?.let {
            sensorManager.registerListener(this, it, SensorManager.SENSOR_DELAY_FASTEST)
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
            Sensor.TYPE_ROTATION_VECTOR,
            Sensor.TYPE_GEOMAGNETIC_ROTATION_VECTOR -> {
                // Convert Android rotation vector → quaternion (w, x, y, z)
                val androidQuat = FloatArray(4)
                SensorManager.getQuaternionFromVector(androidQuat, event.values)
                // setQuaternion re-orders to our wire format (x, y, z, w)
                estimator.setQuaternion(androidQuat)
                onPose(estimator.getQuaternion(), estimator.getLastAccel())
            }
            Sensor.TYPE_ACCELEROMETER -> estimator.onAccel(event.values)
        }
    }

    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {
        Log.d(TAG, "Sensor accuracy changed: ${sensor.name} → $accuracy")
    }
}
