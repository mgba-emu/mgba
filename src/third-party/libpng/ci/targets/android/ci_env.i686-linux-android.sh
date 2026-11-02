# Copyright (C) 2023 Cosmin Truta
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=i686
export CI_TARGET_ARCHVER=i686
export CI_TARGET_SYSTEM=linux
export CI_TARGET_ABI=android
export CI_TARGET_ABIVER=android29

export CI_CC="$CI_TARGET_ARCHVER-$CI_TARGET_SYSTEM-$CI_TARGET_ABIVER-clang"
export CI_AR="llvm-ar"
export CI_RANLIB="llvm-ranlib"
