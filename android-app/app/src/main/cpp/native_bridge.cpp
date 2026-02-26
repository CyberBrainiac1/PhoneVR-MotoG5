// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android JNI Bridge
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// native_bridge.cpp — JNI functions for performance-critical operations.
// Currently provides fast quaternion multiplication and Madgwick filter
// helpers that are too slow in Kotlin for high-frequency sensor callbacks.

#include <jni.h>
#include <android/log.h>
#include <cmath>
#include <cstring>

#define LOG_TAG "PhoneVR-Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" {

/**
 * Multiply two quaternions and return the result.
 * qA and qB are float[4] in x,y,z,w order.
 * qOut must be a float[4] pre-allocated array.
 */
JNIEXPORT void JNICALL
Java_com_phonevrmotog5_tracking_PoseEstimator_quatMultiplyNative(
    JNIEnv* env, jobject /*thiz*/,
    jfloatArray jqA, jfloatArray jqB, jfloatArray jqOut)
{
    jfloat* qA  = env->GetFloatArrayElements(jqA,  nullptr);
    jfloat* qB  = env->GetFloatArrayElements(jqB,  nullptr);
    jfloat* out = env->GetFloatArrayElements(jqOut, nullptr);

    // Hamilton product (x,y,z,w)
    out[0] = qA[3]*qB[0] + qA[0]*qB[3] + qA[1]*qB[2] - qA[2]*qB[1];
    out[1] = qA[3]*qB[1] - qA[0]*qB[2] + qA[1]*qB[3] + qA[2]*qB[0];
    out[2] = qA[3]*qB[2] + qA[0]*qB[1] - qA[1]*qB[0] + qA[2]*qB[3];
    out[3] = qA[3]*qB[3] - qA[0]*qB[0] - qA[1]*qB[1] - qA[2]*qB[2];

    env->ReleaseFloatArrayElements(jqA,  qA,  JNI_ABORT);
    env->ReleaseFloatArrayElements(jqB,  qB,  JNI_ABORT);
    env->ReleaseFloatArrayElements(jqOut, out, 0);
}

/**
 * Integrate gyroscope angular velocity into a quaternion over dt seconds.
 * Modifies jqInOut in place.
 * @param jw     float[3] angular velocity (rad/s) x,y,z
 * @param dt     time step in seconds
 * @param jqInOut float[4] quaternion x,y,z,w (modified in place)
 */
JNIEXPORT void JNICALL
Java_com_phonevrmotog5_tracking_PoseEstimator_integrateGyroNative(
    JNIEnv* env, jobject /*thiz*/,
    jfloatArray jw, jfloat dt, jfloatArray jqInOut)
{
    jfloat* w = env->GetFloatArrayElements(jw,      nullptr);
    jfloat* q = env->GetFloatArrayElements(jqInOut, nullptr);

    float halfDt = dt * 0.5f;
    float wx = w[0], wy = w[1], wz = w[2];
    float qx = q[0], qy = q[1], qz = q[2], qw = q[3];

    q[0] = qx + halfDt * ( qw*wx - qz*wy + qy*wz);
    q[1] = qy + halfDt * ( qz*wx + qw*wy - qx*wz);
    q[2] = qz + halfDt * (-qy*wx + qx*wy + qw*wz);
    q[3] = qw + halfDt * (-qx*wx - qy*wy - qz*wz);

    // Normalize
    float mag = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    if (mag > 1e-6f) {
        q[0] /= mag; q[1] /= mag; q[2] /= mag; q[3] /= mag;
    }

    env->ReleaseFloatArrayElements(jw,      w, JNI_ABORT);
    env->ReleaseFloatArrayElements(jqInOut, q, 0);
}

} // extern "C"
