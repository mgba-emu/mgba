# Android Third-Party Notices

This file tracks notices that are relevant to the native Android port in
`src/platform/android`. It is a release checklist companion to the root
`LICENSE`, root `README.md`, and the license files preserved under
`src/third-party`.

## mGBA

- Component: mGBA emulator core and Android port glue.
- License: Mozilla Public License 2.0.
- Copyright: 2013-2026 Jeffrey Pfau and contributors.
- Source license file: `LICENSE`.

## Bundled Native Source Used By Android Builds

- zlib and MiniZip: zlib license. Source and notices are preserved under
  `src/third-party/zlib`.
- LZMA SDK: public domain. Source headers carry Igor Pavlov public-domain
  notices under `src/third-party/lzma`.
- libpng: libpng license when enabled by a build. Source and notices are
  preserved under `src/third-party/libpng`.
- inih: BSD 3-clause license. Source and license are preserved under
  `src/third-party/inih`.

The Android CMake configuration currently disables desktop-only integrations
such as SDL, Qt, FFmpeg, Discord RPC, Lua, SQLite, and libzip.

## Android Tooling And Platform APIs

- Android Gradle Plugin and Gradle wrapper: Apache License 2.0. The wrapper
  scripts keep their SPDX headers in this directory.
- Android SDK, Android NDK, OpenSL ES, EGL, GLESv2, and Android platform APIs
  are used as platform/toolchain dependencies and are not bundled as app
  source dependencies.
- JUnit is a unit-test dependency only and is not packaged into release APKs or
  AABs.

## Not Currently Declared As Runtime Dependencies

The Android app does not currently declare AndroidX, Oboe, CameraX, Room,
Flutter, React Native, Unity, Electron, or libretro as runtime app
dependencies.

If one of these is added later, update this file, the in-app license dialog,
and the release checklist before publishing an APK or AAB.

## Content Boundary

The Android app does not bundle commercial ROMs, BIOS files, copyrighted test
ROMs, or game artwork. Users provide their own files through Android storage
access flows.
