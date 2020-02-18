# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi  # pylint: disable=no-name-in-module


class SM83Core:
    # pylint: disable=invalid-name
    def __init__(self, native):
        self._native = ffi.cast("struct SM83Core*", native)

    @property
    def a(self):
        return self._native.a

    @property
    def b(self):
        return self._native.b

    @property
    def c(self):
        return self._native.c

    @property
    def d(self):
        return self._native.d

    @property
    def e(self):
        return self._native.e

    @property
    def f(self):
        return self._native.f

    @property
    def h(self):
        return self._native.h

    @property
    def l(self):
        return self._native.l

    @property
    def sp(self):
        return self._native.sp

    @property
    def pc(self):
        return self._native.pc

    @property
    def af(self):
        return (self.a << 8) | self.f

    @property
    def bc(self):
        return (self.b << 8) | self.c

    @property
    def de(self):
        return (self.d << 8) | self.e

    @property
    def hl(self):
        return (self.h << 8) | self.l

    @a.setter
    def a(self, value):
        self._native.a = value

    @b.setter
    def b(self, value):
        self._native.b = value

    @c.setter
    def c(self, value):
        self._native.c = value

    @d.setter
    def d(self, value):
        self._native.d = value

    @e.setter
    def e(self, value):
        self._native.e = value

    @f.setter
    def f(self, value):
        self._native.f.packed = value
        self._native.f.unused = 0

    @h.setter
    def h(self, value):
        self._native.h = value

    @l.setter
    def l(self, value):
        self._native.l = value

    @sp.setter
    def sp(self, value):
        self._native.sp = value

    @af.setter
    def af(self, value):
        self.a = value >> 8
        self.f = value & 0xFF

    @bc.setter
    def bc(self, value):
        self.b = value >> 8
        self.c = value & 0xFF

    @de.setter
    def de(self, value):
        self.d = value >> 8
        self.e = value & 0xFF

    @hl.setter
    def hl(self, value):
        self.h = value >> 8
        self.l = value & 0xFF
