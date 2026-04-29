# mGBA Android

This directory contains the native Android port scaffold for mGBA.

## Toolchain

- Android Gradle Plugin: 9.1.0
- Gradle: 9.3.1
- JDK: 17
- Default NDK: 28.2.13676358

The versions above follow the Android Gradle Plugin 9.1 compatibility table from Android Developers.

## Build

Install Android Studio or configure `ANDROID_HOME` and a compatible Gradle installation, then run:

```bash
cd src/platform/android
gradle :app:assembleDebug
```

The local development machine used to create this scaffold did not have `gradle` or `ANDROID_HOME` configured, so the first checked-in state is intentionally limited to source-level scaffolding.
