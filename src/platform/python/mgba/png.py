# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import vfs

class PNG:
	def __init__(self, f):
		self.vf = vfs.open(f)

	def writeHeader(self, image):
		self._png = lib.PNGWriteOpen(self.vf.handle)
		self._info = lib.PNGWriteHeader(self._png, image.width, image.height)
		return self._info != ffi.NULL

	def writePixels(self, image):
		return lib.PNGWritePixels(self._png, image.width, image.height, image.stride, image.buffer)

	def writeClose(self):
		lib.PNGWriteClose(self._png, self._info)
		del self._png
		del self._info
