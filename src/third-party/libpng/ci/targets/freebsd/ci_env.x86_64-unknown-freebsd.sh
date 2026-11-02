# Copyright (C) 2023 Cosmin Truta
# SPDX-License-Identifier: MIT

export CI_TARGET_ARCH=x86_64
export CI_TARGET_SYSTEM=freebsd

export CI_CMAKE_VARS="
    -DCMAKE_SYSTEM_NAME=FreeBSD
    -DCMAKE_SYSTEM_PROCESSOR=$CI_TARGET_ARCH
"
