# Add project specific ProGuard rules here.
# SPDX-License-Identifier: GPL-3.0-only
# PhoneVR-MotoG5 proguard-rules.pro

# Keep native method names for JNI
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep PoseEstimator for JNI native calls
-keep class com.phonevrmotog5.tracking.PoseEstimator { *; }

# Suppress warnings for unused classes
-dontwarn javax.annotation.**
