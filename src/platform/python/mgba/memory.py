# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib

class MemoryView(object):
    def __init__(self, core, width, size, base=0, sign="u"):
        self._core = core
        self._width = width
        self._size = size
        self._base = base
        self._busRead = getattr(self._core, "busRead" + str(width * 8))
        self._busWrite = getattr(self._core, "busWrite" + str(width * 8))
        self._rawRead = getattr(self._core, "rawRead" + str(width * 8))
        self._rawWrite = getattr(self._core, "rawWrite" + str(width * 8))
        self._mask = (1 << (width * 8)) - 1 # Used to force values to fit within range so that negative values work
        if sign == "u" or sign == "unsigned":
            self._type = "uint{}_t".format(width * 8)
        elif sign == "i" or sign == "s" or sign == "signed":
            self._type = "int{}_t".format(width * 8)
        else:
            raise ValueError("Invalid sign type: '{}'".format(sign))

    def _addrCheck(self, address):
        if isinstance(address, slice):
            start = address.start or 0
            stop = self._size - self._width if address.stop is None else address.stop
        else:
            start = address
            stop = address + self._width
        if start >= self._size or stop > self._size:
            raise IndexError()
        if start < 0 or stop < 0:
            raise IndexError()

    def __len__(self):
        return self._size

    def __getitem__(self, address):
        self._addrCheck(address)
        if isinstance(address, slice):
            start = address.start or 0
            stop = self._size - self._width if address.stop is None else address.stop
            step = address.step or self._width
            return [int(ffi.cast(self._type, self._busRead(self._core, self._base + a))) for a in range(start, stop, step)]
        else:
            return int(ffi.cast(self._type, self._busRead(self._core, self._base + address)))

    def __setitem__(self, address, value):
        self._addrCheck(address)
        if isinstance(address, slice):
            start = address.start or 0
            stop = self._size - self._width if address.stop is None else address.stop
            step = address.step or self._width
            for a in range(start, stop, step):
                self._busWrite(self._core, self._base + a, value[a] & self._mask)
        else:
            self._busWrite(self._core, self._base + address, value & self._mask)

    def rawRead(self, address, segment=-1):
        self._addrCheck(address)
        return int(ffi.cast(self._type, self._rawRead(self._core, self._base + address, segment)))

    def rawWrite(self, address, value, segment=-1):
        self._addrCheck(address)
        self._rawWrite(self._core, self._base + address, segment, value & self._mask)

class Memory(object):
    def __init__(self, core, size, base=0):
        self.size = size
        self.base = base

        self.u8 = MemoryView(core, 1, size, base, "u")
        self.u16 = MemoryView(core, 2, size, base, "u")
        self.u32 = MemoryView(core, 4, size, base, "u")
        self.s8 = MemoryView(core, 1, size, base, "s")
        self.s16 = MemoryView(core, 2, size, base, "s")
        self.s32 = MemoryView(core, 4, size, base, "s")

    def __len__(self):
        return self._size
