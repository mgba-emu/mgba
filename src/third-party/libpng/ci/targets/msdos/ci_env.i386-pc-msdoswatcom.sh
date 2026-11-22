# Copyright (C) 2023 Cosmin Truta
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=i386
export CI_TARGET_SYSTEM=msdoswatcom

export CI_CC="wcl386"

# Open Watcom V2 CMake build
# https://github.com/open-watcom/open-watcom-v2/discussions/716
export CI_CMAKE_GENERATOR="Watcom WMake"
export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=DOS
"
