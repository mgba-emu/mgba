# mGBA Android

This directory contains the native Android port scaffold for mGBA.

## Toolchain

- Android Gradle Plugin: 9.1.0
- Gradle: 9.3.1
- JDK: 17
- Default NDK: 28.2.13676358

The versions above follow the Android Gradle Plugin 9.1 compatibility table from Android Developers.

## Build

Install Android Studio or configure `ANDROID_HOME`, then run:

```bash
cd src/platform/android
./gradlew :app:assembleDebug
```

The checked-in Gradle wrapper pins Gradle 9.3.1 for reproducible local and CI builds.
