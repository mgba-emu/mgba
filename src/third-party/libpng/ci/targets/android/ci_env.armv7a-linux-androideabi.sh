# Copyright (C) 2023 Cosmin Truta
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=arm
export CI_TARGET_ARCHVER=armv7a
export CI_TARGET_SYSTEM=linux
export CI_TARGET_ABI=androideabi
export CI_TARGET_ABIVER=androideabi29

export CI_CC="$CI_TARGET_ARCHVER-$CI_TARGET_SYSTEM-$CI_TARGET_ABIVER-clang"
export CI_AR="llvm-ar"
export CI_RANLIB="llvm-ranlib"
