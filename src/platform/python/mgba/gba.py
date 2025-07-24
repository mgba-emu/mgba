# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from .arm import ARMCore
from .core import Core, needs_reset
from .tile import Sprite
from .memory import Memory
from . import create_callback


class GBA(Core):
    KEY_A = lib.GBA_KEY_A
    KEY_B = lib.GBA_KEY_B
    KEY_SELECT = lib.GBA_KEY_SELECT
    KEY_START = lib.GBA_KEY_START
    KEY_DOWN = lib.GBA_KEY_DOWN
    KEY_UP = lib.GBA_KEY_UP
    KEY_LEFT = lib.GBA_KEY_LEFT
    KEY_RIGHT = lib.GBA_KEY_RIGHT
    KEY_L = lib.GBA_KEY_L
    KEY_R = lib.GBA_KEY_R

    SIO_NORMAL_8 = lib.GBA_SIO_NORMAL_8
    SIO_NORMAL_32 = lib.GBA_SIO_NORMAL_32
    SIO_MULTI = lib.GBA_SIO_MULTI
    SIO_UART = lib.GBA_SIO_UART
    SIO_JOYBUS = lib.GBA_SIO_JOYBUS
    SIO_GPIO = lib.GBA_SIO_GPIO

    def __init__(self, native):
        super(GBA, self).__init__(native)
        self._native = ffi.cast("struct GBA*", native.board)
        self.sprites = GBAObjs(self)
        self.cpu = ARMCore(self._core.cpu)
        self.memory = None
        self._sio = set()

    @needs_reset
    def _init_cache(self, cache):
        lib.GBAVideoCacheInit(cache)
        lib.GBAVideoCacheAssociate(cache, ffi.addressof(self._native.video))

    def _deinit_cache(self, cache):
        lib.mCacheSetDeinit(cache)
        if self._was_reset:
            self._native.video.renderer.cache = ffi.NULL

    def _load(self):
        super(GBA, self)._load()
        self.memory = GBAMemory(self._core, self._native.memory.romSize)


class GBAMemory(Memory):
    def __init__(self, core, romSize=lib.GBA_SIZE_ROM0):
        super(GBAMemory, self).__init__(core, 0x100000000)

        self.bios = Memory(core, lib.GBA_SIZE_BIOS, lib.GBA_BASE_BIOS)
        self.wram = Memory(core, lib.GBA_SIZE_EWRAM, lib.GBA_BASE_EWRAM)
        self.iwram = Memory(core, lib.GBA_SIZE_IWRAM, lib.GBA_BASE_IWRAM)
        self.io = Memory(core, lib.GBA_SIZE_IO, lib.GBA_BASE_IO)  # pylint: disable=invalid-name
        self.palette = Memory(core, lib.GBA_SIZE_PALETTE_RAM, lib.GBA_BASE_PALETTE_RAM)
        self.vram = Memory(core, lib.GBA_SIZE_VRAM, lib.GBA_BASE_VRAM)
        self.oam = Memory(core, lib.GBA_SIZE_OAM, lib.GBA_BASE_OAM)
        self.rom0 = Memory(core, romSize, lib.GBA_BASE_ROM0)
        self.rom1 = Memory(core, romSize, lib.GBA_BASE_ROM1)
        self.rom2 = Memory(core, romSize, lib.GBA_BASE_ROM2)
        self.rom = self.rom0
        self.sram = Memory(core, lib.GBA_SIZE_SRAM, lib.GBA_BASE_SRAM)


class GBASprite(Sprite):
    TILE_BASE = 0x800, 0x400
    PALETTE_BASE = 0x10, 1

    def __init__(self, obj):
        self._a = obj.a
        self._b = obj.b
        self._c = obj.c
        self.x = self._b & 0x1FF  # pylint: disable=invalid-name
        self.y = self._a & 0xFF  # pylint: disable=invalid-name
        self._shape = self._a >> 14
        self._size = self._b >> 14
        self._256_color = bool(self._a & 0x2000)
        self.width, self.height = lib.GBAVideoObjSizes[self._shape * 4 + self._size]
        self.tile = self._c & 0x3FF
        if self._256_color:
            self.palette_id = 0
            self.tile >>= 1
        else:
            self.palette_id = self._c >> 12


class GBAObjs:
    def __init__(self, core):
        self._core = core
        self._obj = core._native.video.oam.obj

    def __len__(self):
        return 128

    def __getitem__(self, index):
        if index >= len(self):
            raise IndexError()
        sprite = GBASprite(self._obj[index])
        tiles = self._core.tiles[3 if sprite._256_color else 2]
        map_1d = bool(self._core._native.memory.io[0] & 0x40)
        sprite.constitute(tiles, 0 if map_1d else 0x20)
        return sprite
