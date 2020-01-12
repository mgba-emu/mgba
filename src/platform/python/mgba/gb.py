# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from .sm83 import SM83Core
from .core import Core, needs_reset
from .memory import Memory
from .tile import Sprite
from . import create_callback


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
        self.cpu = SM83Core(self._core.cpu)
        self.memory = None
        self._link = None

    @needs_reset
    def _init_cache(self, cache):
        lib.GBVideoCacheInit(cache)
        lib.GBVideoCacheAssociate(cache, ffi.addressof(self._native.video))

    def _deinit_cache(self, cache):
        lib.mCacheSetDeinit(cache)
        if self._was_reset:
            self._native.video.renderer.cache = ffi.NULL

    def _load(self):
        super(GB, self)._load()
        self.memory = GBMemory(self._core)

    def attach_sio(self, link):
        self._link = link
        lib.GBSIOSetDriver(ffi.addressof(self._native.sio), link._native)

    def __del__(self):
        if self._link:
            lib.GBSIOSetDriver(ffi.addressof(self._native.sio), ffi.NULL)
            self._link = None


create_callback("GBSIOPythonDriver", "init")
create_callback("GBSIOPythonDriver", "deinit")
create_callback("GBSIOPythonDriver", "writeSB")
create_callback("GBSIOPythonDriver", "writeSC")


class GBSIODriver(object):
    def __init__(self):
        self._handle = ffi.new_handle(self)
        self._native = ffi.gc(lib.GBSIOPythonDriverCreate(self._handle), lib.free)

    def init(self):
        return True

    def deinit(self):
        pass

    def write_sb(self, value):
        pass

    def write_sc(self, value):
        return value


class GBSIOSimpleDriver(GBSIODriver):
    def __init__(self, period=0x100):
        super(GBSIOSimpleDriver, self).__init__()
        self.rx = 0x00  # pylint: disable=invalid-name
        self._period = period

    def init(self):
        self._native.p.period = self._period
        return True

    def write_sb(self, value):
        self.rx = value  # pylint: disable=invalid-name

    def write_sc(self, value):
        self._native.p.period = self._period
        if value & 0x80:
            lib.mTimingDeschedule(ffi.addressof(self._native.p.p.timing), ffi.addressof(self._native.p.event))
            lib.mTimingSchedule(ffi.addressof(self._native.p.p.timing), ffi.addressof(self._native.p.event), self._native.p.period)
        return value

    def is_ready(self):
        return not self._native.p.remainingBits

    @property
    def tx(self):  # pylint: disable=invalid-name
        return self._native.p.pendingSB

    @property
    def period(self):
        return self._native.p.period

    @tx.setter
    def tx(self, newTx):  # pylint: disable=invalid-name
        self._native.p.pendingSB = newTx
        self._native.p.remainingBits = 8

    @period.setter
    def period(self, new_period):
        self._period = new_period
        if self._native.p:
            self._native.p.period = new_period


class GBMemory(Memory):
    def __init__(self, core):
        super(GBMemory, self).__init__(core, 0x10000)

        self.cart = Memory(core, lib.GB_SIZE_CART_BANK0 * 2, lib.GB_BASE_CART_BANK0)
        self.vram = Memory(core, lib.GB_SIZE_VRAM, lib.GB_BASE_VRAM)
        self.sram = Memory(core, lib.GB_SIZE_EXTERNAL_RAM, lib.GB_REGION_EXTERNAL_RAM)
        self.iwram = Memory(core, lib.GB_SIZE_WORKING_RAM_BANK0, lib.GB_BASE_WORKING_RAM_BANK0)
        self.oam = Memory(core, lib.GB_SIZE_OAM, lib.GB_BASE_OAM)
        self.io = Memory(core, lib.GB_SIZE_IO, lib.GB_BASE_IO)  # pylint: disable=invalid-name
        self.hram = Memory(core, lib.GB_SIZE_HRAM, lib.GB_BASE_HRAM)


class GBSprite(Sprite):
    PALETTE_BASE = (8,)

    def __init__(self, obj, core):
        self.x = obj.x  # pylint: disable=invalid-name
        self.y = obj.y  # pylint: disable=invalid-name
        self.tile = obj.tile
        self._attr = obj.attr
        self.width = 8
        lcdc = core._native.memory.io[0x40]
        self.height = 16 if lcdc & 4 else 8
        if core._native.model >= lib.GB_MODEL_CGB:
            if self._attr & 8:
                self.tile += 512
            self.palette_id = self._attr & 7
        else:
            self.palette_id = (self._attr >> 4) & 1
        self.palette_id += 8


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
