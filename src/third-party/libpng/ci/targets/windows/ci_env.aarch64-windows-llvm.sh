# Copyright (C) 2023 Cosmin Truta
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=aarch64
export CI_TARGET_SYSTEM=windows

export CI_CC="clang"
export CI_AR="llvm-ar"
export CI_RANLIB="llvm-ranlib"

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=Windows
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
"
