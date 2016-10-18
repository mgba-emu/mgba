# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import image

class Tile:
	def __init__(self, data):
		self.buffer = data

	def toImage(self):
		i = image.Image(8, 8)
		self.composite(i, 0, 0)
		return i

	def composite(self, i, x, y):
		for iy in range(8):
			for ix in range(8):
				i.buffer[ix + x + (iy + y) * i.stride] = image.u16ToColor(self.buffer[ix + iy * 8])

class TileView:
	def __init__(self, core):
		self.core = core
		self.cache = ffi.gc(ffi.new("struct mTileCache*"), lib.mTileCacheDeinit)
		if core.platform() == core.PLATFORM_GBA:
			lib.GBAVideoTileCacheInit(self.cache)
			lib.GBAVideoTileCacheAssociate(self.cache, ffi.addressof(self.core.board._native, "video"))
		if core.platform() == core.PLATFORM_GB:
			lib.GBVideoTileCacheInit(self.cache)
			lib.GBVideoTileCacheAssociate(self.cache, ffi.addressof(self.core.board._native, "video"))
		lib.mTileCacheSetPalette(self.cache, 0)

	def getTile(self, tile, palette):
		return Tile(lib.mTileCacheGetTile(self.cache, tile, palette))
