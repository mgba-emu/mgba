# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from .arm import ARMCore
from .core import Core
from .tile import Sprite

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

    def __init__(self, native):
        super(GBA, self).__init__(native)
        self._native = ffi.cast("struct GBA*", native.board)
        self.sprites = GBAObjs(self)
        self.cpu = ARMCore(self._core.cpu)

    def _initTileCache(self, cache):
        lib.GBAVideoTileCacheInit(cache)
        lib.GBAVideoTileCacheAssociate(cache, ffi.addressof(self._native.video))

    def _deinitTileCache(self, cache):
        self._native.video.renderer.cache = ffi.NULL
        lib.mTileCacheDeinit(cache)

class GBASprite(Sprite):
    TILE_BASE = 0x800, 0x400
    PALETTE_BASE = 0x10, 1

    def __init__(self, obj):
        self._a = obj.a
        self._b = obj.b
        self._c = obj.c
        self.x = self._b & 0x1FF
        self.y = self._a & 0xFF
        self._shape = self._a >> 14
        self._size = self._b >> 14
        self._256Color = bool(self._a & 0x2000)
        self.width, self.height = lib.GBAVideoObjSizes[self._shape * 4 + self._size]
        self.tile = self._c & 0x3FF
        if self._256Color:
            self.paletteId = 0
            self.tile >>= 1
        else:
            self.paletteId = self._c >> 12

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
        map1D = bool(self._core._native.memory.io[0] & 0x40)
        sprite.constitute(self._core.tiles, 0 if map1D else 0x20, int(sprite._256Color))
        return sprite
