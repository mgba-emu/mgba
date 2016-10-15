# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib

class _ARMRegisters:
    def __init__(self, cpu):
        self._cpu = cpu

    def __getitem__(self, r):
        if r > lib.ARM_PC:
            raise IndexError("Register out of range")
        return self._cpu._native.gprs[r]

    def __setitem__(self, r, value):
        if r >= lib.ARM_PC:
            raise IndexError("Register out of range")
        self._cpu._native.gprs[r] = value

class ARMCore:
    def __init__(self, native):
        self._native = ffi.cast("struct ARMCore*", native)
        self.gprs = _ARMRegisters(self)
        self.cpsr = self._native.cpsr
        self.spsr = self._native.spsr

