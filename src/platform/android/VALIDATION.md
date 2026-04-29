# Android Validation Checklist

This checklist is the repeatable manual validation flow for the native Android port.
Use public-domain or homebrew ROMs only. Do not copy commercial ROMs, user BIOS files,
or save data into the repository.

## Local Environment

If Java or the Android SDK are not already visible to the shell, export them before
running Gradle:

```bash
export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
export ANDROID_HOME="$HOME/Library/Android/sdk"
```

## Latest Automated Pass

2026-04-30 on `master`:

```bash
scripts/check-bundled-assets.sh

JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
ANDROID_HOME="$HOME/Library/Android/sdk" \
./gradlew :app:assembleDebug :app:testDebugUnitTest \
  :app:externalNativeBuildDebug -PmgbaAndroidWarningsAsErrors=true \
  :app:assembleRelease :app:bundleRelease --no-daemon

JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
ANDROID_HOME="$HOME/Library/Android/sdk" \
./gradlew :app:connectedDebugAndroidTest --no-daemon
```

Result:

- Bundled asset check passed.
- Debug APK, release APK, and release AAB built successfully.
- Unit tests passed.
- Android JNI/native wrapper warning check passed.
- Instrumented tests passed on `Medium_Phone(AVD) - 16`, `sdk_gphone64_arm64`.

## Build Artifacts

Run these from `src/platform/android` before installing:

```bash
scripts/check-bundled-assets.sh
./gradlew :app:assembleDebug :app:testDebugUnitTest --no-daemon
./gradlew :app:externalNativeBuildDebug -PmgbaAndroidWarningsAsErrors=true --no-daemon
./gradlew :app:assembleRelease :app:bundleRelease --no-daemon
```

Expected result:

- Debug APK builds.
- Release APK and AAB build.
- Unit tests pass.
- Native wrapper warning check passes.
- CI bundled asset check finds no ROM, save, state, or BIOS files under `app/src/main`.

## Device Matrix

Run the gameplay flow on each available class of device:

- arm64 mid-range phone.
- arm64 low-end phone.
- Android tablet.
- x86_64 Android emulator.
- At least one external Bluetooth controller.
- At least one Bluetooth audio route.

Record Android version, device model, ABI, display mode, and controller/audio device names.

## Install And Launch

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n io.mgba.android/.MainActivity
```

Expected result:

- MainActivity opens without a crash.
- Native version label is populated.
- About and Licenses dialogs open.
- Storage dialog shows app data and cache sizes.
- Export Logs creates or prompts for a log file.

## ROM Loading

Use SAF pickers, not repository files:

- Open a `.gba` ROM.
- Open a `.gb` ROM.
- Open a `.gbc` ROM.
- Open a single-ROM `.zip`.
- Open a multi-ROM `.zip` and choose an entry.
- Open a single-ROM `.7z`.
- Try an invalid file and verify a clear failure message.

Expected result:

- EmulatorActivity starts for valid ROMs.
- First nonblank frame appears.
- Audio starts unless muted.
- Recent list updates.
- Library last-played metadata updates.
- Invalid files do not crash the app.

## Video And Lifecycle

For GBA and GB/GBC:

- Toggle Fit, Fill, Integer, Original, and Stretch.
- Toggle Pixel and Smooth filtering.
- Toggle interframe blending.
- Rotate between portrait and landscape.
- Lock screen and unlock.
- Send app to background and resume.
- Destroy/recreate Surface by switching apps and rotating.

Expected result:

- Aspect ratio and viewport recover.
- No black screen after resume.
- Audio pauses in background and resumes in foreground.
- Core state is preserved.

## Input

Touch input:

- Press D-pad plus A/B simultaneously.
- Hold L/R and Start/Select.
- Toggle virtual gamepad visibility.
- Change size, opacity, spacing, handedness, and layout edit offsets.

Hardware input:

- Press face buttons, D-pad, shoulders, Start/Select.
- Move sticks and hat axes.
- Use trigger axes for L/R.
- Change deadzone.
- Remap at least one button, export profile, reset, and import profile.

Expected result:

- No stuck buttons after pause, resume, or touch cancel.
- Remaps persist across app restart.
- Debug input panel reports device, key, axis, and mapped mask.

## Save, State, Screenshot

For a loaded ROM:

- Import a `.sav` file.
- Export current battery save.
- Save and load each state slot 1-9.
- Delete a state slot.
- Export and import one state slot.
- Confirm state thumbnail updates after save/import/delete.
- Take a screenshot and share it.
- Export a screenshot via MediaStore or SAF fallback.
- Enable auto-state on exit and relaunch.

Expected result:

- Saves and states survive app restart.
- Import prompts before overwriting an occupied state slot.
- Exported files can be re-imported.
- Screenshot files are valid PNGs.

## BIOS, Patch, Cheats, Sensors

- Import and clear global BIOS slots.
- Import and clear per-game BIOS slots.
- Toggle Skip BIOS and relaunch/reset.
- Import IPS, UPS, and BPS patches where applicable.
- Verify auto patch lookup by ROM name and CRC32.
- Import cheats, toggle entries, edit entries, add a manual entry, and delete one.
- Toggle rumble and verify haptic pulses on supported games.
- Toggle tilt, calibrate, and observe debug/runtime behavior.
- Adjust solar level manually and with light sensor if available.
- Import and clear Game Boy Camera static image source.

Expected result:

- Per-game artifacts are keyed by stable CRC32 id when available.
- Runtime changes persist where intended.
- Game data package export/import preserves per-game settings and artifacts.

## Diagnostics

During gameplay:

- Toggle debug stats overlay.
- Export runtime diagnostics.
- Export app logs after normal play.
- Force-close and reopen the app, then check crash recovery prompt if applicable.

Expected result:

- FPS, frame time, video size, audio underruns, and ROM metadata are visible.
- Diagnostics include runtime stats and app log ring buffer.
- Log export works on Android 6-9 via SAF and Android 10+ via MediaStore.
