# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from .lr35902 import LR35902Core
from .core import Core
from .tile import Sprite

class GB(Core):
    def __init__(self, native):
        super(GB, self).__init__(native)
        self._native = ffi.cast("struct GB*", native.board)
        self.sprites = GBObjs(self)
        self.cpu = LR35902Core(self._core.cpu)

    def _initTileCache(self, cache):
        lib.GBVideoTileCacheInit(cache)
        lib.GBVideoTileCacheAssociate(cache, ffi.addressof(self._native.video))

    def _deinitTileCache(self, cache):
        self._native.video.renderer.cache = ffi.NULL
        lib.mTileCacheDeinit(cache)

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
        sprite.constitute(self._core.tiles, 0, 0)
        return sprite
