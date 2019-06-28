# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from . import image


class Tile:
    def __init__(self, data):
        self.buffer = data

    def to_image(self):
        i = image.Image(8, 8)
        self.composite(i, 0, 0)
        return i

    def composite(self, i, x, y):
        for iy in range(8):
            ffi.memmove(ffi.addressof(i.buffer, x + (iy + y) * i.stride), ffi.addressof(self.buffer, iy * 8), 8 * ffi.sizeof("color_t"))


class CacheSet:
    def __init__(self, core):
        self.core = core
        self.cache = ffi.gc(ffi.new("struct mCacheSet*"), core._deinit_cache)
        core._init_cache(self.cache)


class TileView:
    def __init__(self, cache):
        self.cache = cache

    def get_tile(self, tile, palette):
        return Tile(lib.mTileCacheGetTile(self.cache, tile, palette))


class MapView:
    def __init__(self, cache):
        self.cache = cache

    @property
    def width(self):
        return 1 << lib.mMapCacheSystemInfoGetTilesWide(self.cache.sysConfig)

    @property
    def height(self):
        return 1 << lib.mMapCacheSystemInfoGetTilesHigh(self.cache.sysConfig)

    @property
    def image(self):
        i = image.Image(self.width * 8, self.height * 8, alpha=True)
        for y in range(self.height * 8):
            if not y & 7:
                lib.mMapCacheCleanRow(self.cache, y >> 3)
            row = lib.mMapCacheGetRow(self.cache, y)
            ffi.memmove(ffi.addressof(i.buffer, i.stride * y), row, self.width * 8 * ffi.sizeof("color_t"))
        return i


class Sprite(object):
    def constitute(self, tileView, tilePitch):
        i = image.Image(self.width, self.height, alpha=True)
        tileId = self.tile
        for y in range(self.height // 8):
            for x in range(self.width // 8):
                tile = tileView.getTile(tileId, self.paletteId)
                tile.composite(i, x * 8, y * 8)
                tileId += 1
            if tilePitch:
                tileId += tilePitch - self.width // 8
        self.image = i
