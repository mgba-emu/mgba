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

## Native warning check

CI treats Android platform native warnings as errors with:

```bash
./gradlew :app:externalNativeBuildDebug -PmgbaAndroidWarningsAsErrors=true
```

This strict mode applies to the Android JNI/native wrapper target. It can also be enabled with `MGBA_ANDROID_WARNINGS_AS_ERRORS=true`.

## Release signing

Release builds are unsigned unless signing inputs are provided. Configure either Gradle properties or matching environment variables:

| Gradle property | Environment variable |
| --- | --- |
| `mgbaAndroidKeystoreFile` | `MGBA_ANDROID_KEYSTORE_FILE` |
| `mgbaAndroidKeystoreBase64` | `MGBA_ANDROID_KEYSTORE_BASE64` |
| `mgbaAndroidKeystorePassword` | `MGBA_ANDROID_KEYSTORE_PASSWORD` |
| `mgbaAndroidKeyAlias` | `MGBA_ANDROID_KEY_ALIAS` |
| `mgbaAndroidKeyPassword` | `MGBA_ANDROID_KEY_PASSWORD` |

`mgbaAndroidKeystoreFile` points to a local keystore path. `mgbaAndroidKeystoreBase64` is useful for CI secrets and is decoded into the build directory at configuration time. When all required values are present, `:app:assembleRelease` and `:app:bundleRelease` use the release signing config automatically.

## Crash symbolication

Release native symbols are produced at `app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip` and uploaded by CI as `mgba-android-release-native-symbols`. See [CRASH_SYMBOLS.md](CRASH_SYMBOLS.md) for the local and internal release crash debug flow.
