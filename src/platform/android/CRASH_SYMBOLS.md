# Android Crash Symbolication

This document describes the release crash debug flow for the native Android port.

## Artifacts

The Android CI workflow uploads these artifacts on every Android build:

- `mgba-android-release-apk`
- `mgba-android-release-aab`
- `mgba-android-release-native-symbols`

The native symbols artifact points at:

```text
src/platform/android/app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip
```

Keep the release APK or AAB together with the matching native symbols zip. Symbol files must come from the same commit and build variant as the crashing binary.

## Device Log Export

From the app start screen, use the log export action after a crash prompt or from the log button. The export writes a text file to:

```text
Documents/mGBA/mgba-log-<timestamp>.txt
```

For local debug builds, `adb logcat -d -t 1000` can also be collected directly. Native crashes usually contain a `backtrace` block with ABI, library name, and program counter offsets.

## Local Symbolication

Use the NDK tools from the same Android SDK installation used to build the APK.

```bash
cd src/platform/android
unzip app/build/outputs/native-debug-symbols/release/native-debug-symbols.zip -d /tmp/mgba-symbols
```

For a logcat crash dump:

```bash
$ANDROID_HOME/ndk/28.2.13676358/ndk-stack \
  -sym /tmp/mgba-symbols \
  -dump /path/to/mgba-log.txt
```

For a single program counter offset, use the ABI-specific shared object:

```bash
$ANDROID_HOME/ndk/28.2.13676358/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-addr2line \
  -Cfipe /tmp/mgba-symbols/arm64-v8a/libmgba-android.so \
  0x0000000000000000
```

Replace the ABI and address with the values from the crash backtrace. On Linux CI hosts, replace `darwin-x86_64` with the matching prebuilt host directory.

## Play/Internal Release Flow

For internal release distribution:

1. Build `:app:bundleRelease` from a clean commit.
2. Store the generated AAB and `native-debug-symbols.zip` together.
3. Upload the AAB to the internal track.
4. Upload `native-debug-symbols.zip` as the native debug symbols for that exact version.
5. When a crash report arrives, match it by version code, ABI, and build commit before symbolication.

If release signing is configured through Gradle properties or environment variables, the signing config is applied automatically during `assembleRelease` and `bundleRelease`.
