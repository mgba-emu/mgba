# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib

class LR35902Core:
    def __init__(self, native):
        self._native = ffi.cast("struct LR35902Core*", native)

    def __getattr__(self, key):
        if key == 'a':
            return self._native.a
        if key == 'b':
            return self._native.b
        if key == 'c':
            return self._native.c
        if key == 'd':
            return self._native.d
        if key == 'e':
            return self._native.e
        if key == 'f':
            return self._native.f
        if key == 'h':
            return self._native.h
        if key == 'l':
            return self._native.l
        if key == 'sp':
            return self._native.sp
        if key == 'pc':
            return self._native.pc
        raise AttributeError()

    def __setattr__(self, key, value):
        if key == 'a':
            self._native.a = value & 0xF0
        if key == 'b':
            self._native.b = value
        if key == 'c':
            self._native.c = value
        if key == 'd':
            self._native.d = value
        if key == 'e':
            self._native.e = value
        if key == 'f':
            self._native.f = value
        if key == 'h':
            self._native.h = value
        if key == 'l':
            self._native.l = value
        if key == 'sp':
            self._native.sp = value
        else:
            self.__dict__[key] = value
