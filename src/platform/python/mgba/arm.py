# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module


class _ARMRegisters:
    def __init__(self, cpu):
        self._cpu = cpu

    def __getitem__(self, reg):
        if reg > lib.ARM_PC:
            raise IndexError("Register out of range")
        return self._cpu._native.gprs[reg]

    def __setitem__(self, reg, value):
        if reg >= lib.ARM_PC:
            raise IndexError("Register out of range")
        self._cpu._native.gprs[reg] = value


class ARMCore:
    SP = 13
    LR = 14
    PC = 15

    def __init__(self, native):
        self._native = ffi.cast("struct ARMCore*", native)
        self.gprs = _ARMRegisters(self)
        self.cpsr = self._native.cpsr
        self.spsr = self._native.spsr

    @property
    def sp(self):
        return self.gprs[self.SP]

    @sp.setter
    def sp(self, value):
        self.gprs[self.SP] = value

    @property
    def lr(self):
        return self.gprs[self.LR]

    @lr.setter
    def lr(self, value):
        self.gprs[self.LR] = value

    @property
    def pc(self):
        return self.gprs[self.PC]

    @pc.setter
    def pc(self, value):
        self.gprs[self.PC] = value
