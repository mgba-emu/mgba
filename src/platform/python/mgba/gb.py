# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from .lr35902 import LR35902Core
from .core import Core, needsReset
from .memory import Memory
from .tile import Sprite
from . import createCallback

class GB(Core):
    KEY_A = lib.GBA_KEY_A
    KEY_B = lib.GBA_KEY_B
    KEY_SELECT = lib.GBA_KEY_SELECT
    KEY_START = lib.GBA_KEY_START
    KEY_DOWN = lib.GBA_KEY_DOWN
    KEY_UP = lib.GBA_KEY_UP
    KEY_LEFT = lib.GBA_KEY_LEFT
    KEY_RIGHT = lib.GBA_KEY_RIGHT

    def __init__(self, native):
        super(GB, self).__init__(native)
        self._native = ffi.cast("struct GB*", native.board)
        self.sprites = GBObjs(self)
        self.cpu = LR35902Core(self._core.cpu)

    @needsReset
    def _initCache(self, cache):
        lib.GBVideoCacheInit(cache)
        lib.GBVideoCacheAssociate(cache, ffi.addressof(self._native.video))

    def _deinitCache(self, cache):
        lib.mCacheSetDeinit(cache)
        if self._wasReset:
            self._native.video.renderer.cache = ffi.NULL

    def _load(self):
        super(GB, self)._load()
        self.memory = GBMemory(self._core)

    def attachSIO(self, link):
        lib.GBSIOSetDriver(ffi.addressof(self._native.sio), link._native)

    def __del__(self):
        lib.GBSIOSetDriver(ffi.addressof(self._native.sio), ffi.NULL)

createCallback("GBSIOPythonDriver", "init")
createCallback("GBSIOPythonDriver", "deinit")
createCallback("GBSIOPythonDriver", "writeSB")
createCallback("GBSIOPythonDriver", "writeSC")

class GBSIODriver(object):
    def __init__(self):
        self._handle = ffi.new_handle(self)
        self._native = ffi.gc(lib.GBSIOPythonDriverCreate(self._handle), lib.free)

    def init(self):
        return True

    def deinit(self):
        pass

    def writeSB(self, value):
        pass

    def writeSC(self, value):
        return value

class GBSIOSimpleDriver(GBSIODriver):
    def __init__(self, period=0x100):
        super(GBSIOSimpleDriver, self).__init__()
        self.rx = 0x00
        self._period = period

    def init(self):
        self._native.p.period = self._period
        return True

    def writeSB(self, value):
        self.rx = value

    def writeSC(self, value):
        self._native.p.period = self._period
        if value & 0x80:
            lib.mTimingDeschedule(ffi.addressof(self._native.p.p.timing), ffi.addressof(self._native.p.event))
            lib.mTimingSchedule(ffi.addressof(self._native.p.p.timing), ffi.addressof(self._native.p.event), self._native.p.period)
        return value

    def isReady(self):
        return not self._native.p.remainingBits

    @property
    def tx(self):
        self._native.p.pendingSB

    @property
    def period(self):
        return self._native.p.period

    @tx.setter
    def tx(self, newTx):
        self._native.p.pendingSB = newTx
        self._native.p.remainingBits = 8

    @period.setter
    def period(self, newPeriod):
        self._period = newPeriod
        if self._native.p:
            self._native.p.period = newPeriod

class GBMemory(Memory):
    def __init__(self, core):
        super(GBMemory, self).__init__(core, 0x10000)

        self.cart = Memory(core, lib.GB_SIZE_CART_BANK0 * 2, lib.GB_BASE_CART_BANK0)
        self.vram = Memory(core, lib.GB_SIZE_VRAM, lib.GB_BASE_VRAM)
        self.sram = Memory(core, lib.GB_SIZE_EXTERNAL_RAM, lib.GB_REGION_EXTERNAL_RAM)
        self.iwram = Memory(core, lib.GB_SIZE_WORKING_RAM_BANK0, lib.GB_BASE_WORKING_RAM_BANK0)
        self.oam = Memory(core, lib.GB_SIZE_OAM, lib.GB_BASE_OAM)
        self.io = Memory(core, lib.GB_SIZE_IO, lib.GB_BASE_IO)
        self.hram = Memory(core, lib.GB_SIZE_HRAM, lib.GB_BASE_HRAM)

class GBSprite(Sprite):
    PALETTE_BASE = 8,

    def __init__(self, obj, core):
        self.x = obj.x
        self.y = obj.y
        self.tile = obj.tile
        self._attr = obj.attr
        self.width = 8
        lcdc = core._native.memory.io[0x40]
        self.height = 16 if lcdc & 4 else 8
        if core._native.model >= lib.GB_MODEL_CGB:
            if self._attr & 8:
                self.tile += 512
            self.paletteId = self._attr & 7
        else:
            self.paletteId = (self._attr >> 4) & 1
        self.paletteId += 8


class GBObjs:
    def __init__(self, core):
        self._core = core
        self._obj = core._native.video.oam.obj

    def __len__(self):
        return 40

    def __getitem__(self, index):
        if index >= len(self):
            raise IndexError()
        sprite = GBSprite(self._obj[index], self._core)
        sprite.constitute(self._core.tiles[0], 0)
        return sprite
