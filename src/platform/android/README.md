# mGBA Android

This directory contains the native Android app for mGBA. It uses Android
Activity/View APIs for the shell, JNI/NDK for the emulator bridge, and the
existing mGBA C/C++ core for emulation.

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

Before cutting release artifacts locally, run the same gate used by CI:

```bash
scripts/check-bundled-assets.sh
./gradlew :app:assembleDebug :app:testDebugUnitTest --no-daemon
./gradlew :app:externalNativeBuildDebug -PmgbaAndroidWarningsAsErrors=true --no-daemon
./gradlew :app:lintDebug --no-daemon
./gradlew :app:assembleRelease :app:bundleRelease --no-daemon
```

To build a subset of ABIs locally or in CI, pass a comma-separated filter:

```bash
./gradlew :app:externalNativeBuildDebug -PmgbaAndroidAbiFilters=arm64-v8a
```

## Native warning check

CI treats Android platform native warnings as errors with:

```bash
./gradlew :app:externalNativeBuildDebug -PmgbaAndroidWarningsAsErrors=true
```

This strict mode applies to the Android JNI/native wrapper target. It can also be enabled with `MGBA_ANDROID_WARNINGS_AS_ERRORS=true`.

## Android lint

Debug lint runs in CI and during release validation. Non-blocking first-release
warnings are documented in `app/lint.xml` so the report stays focused on issues
that require action before shipping.

## Optional GDB stub build

The Android native build keeps mGBA debugger and GDB stub support disabled by default so ordinary debug and release APKs do not expose a listening debug server or carry the extra debugger surface. For internal development builds, compile the native core with debugger support by passing:

```bash
./gradlew :app:externalNativeBuildDebug \
  -PmgbaAndroidEnableGdbStub=true \
  -PmgbaAndroidAbiFilters=arm64-v8a
```

The same switch is available as `MGBA_ANDROID_ENABLE_GDB_STUB=true`. When compiled in, the emulator Run Options panel exposes a `GDB` control for the active game. It listens on `127.0.0.1:2345`, reports the active port in the button label and diagnostics export, and asks for explicit confirmation in non-debuggable builds before enabling the listener.

## Third-party notices

Android-specific release notices are tracked in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Keep that file and the in-app Licenses dialog in sync whenever runtime or bundled source dependencies change.

CI also rejects ROM, archive, save-state, battery-save, and BIOS files under `app/src/main` so generated release artifacts do not accidentally bundle redistributability-sensitive game data. Run `scripts/check-bundled-assets.sh` locally before release builds to use the same check.

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

## Validation

Use [VALIDATION.md](VALIDATION.md) for the repeatable device, emulator, storage, input, save-state, screenshot, sensor, and diagnostics validation flow.
