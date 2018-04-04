# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import vfs

MODE_RGB = 0
MODE_RGBA = 1
MODE_INDEX = 2

class PNG:
	def __init__(self, f, mode=MODE_RGB):
		self.vf = vfs.open(f)
		self.mode = mode

	def writeHeader(self, image):
		self._png = lib.PNGWriteOpen(self.vf.handle)
		if self.mode == MODE_RGB:
			self._info = lib.PNGWriteHeader(self._png, image.width, image.height)
		if self.mode == MODE_RGBA:
			self._info = lib.PNGWriteHeaderA(self._png, image.width, image.height)
		if self.mode == MODE_INDEX:
			self._info = lib.PNGWriteHeader8(self._png, image.width, image.height)
		return self._info != ffi.NULL

	def writePixels(self, image):
		if self.mode == MODE_RGB:
			return lib.PNGWritePixels(self._png, image.width, image.height, image.stride, image.buffer)
		if self.mode == MODE_RGBA:
			return lib.PNGWritePixelsA(self._png, image.width, image.height, image.stride, image.buffer)
		if self.mode == MODE_INDEX:
			return lib.PNGWritePixels8(self._png, image.width, image.height, image.stride, image.buffer)

	def writeClose(self):
		lib.PNGWriteClose(self._png, self._info)
		del self._png
		del self._info
