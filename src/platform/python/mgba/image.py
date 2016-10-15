# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import png

class Image:
	def __init__(self, width, height, stride=0):
		self.width = width
		self.height = height
		self.stride = stride
		self.constitute()

	def constitute(self):
		if self.stride <= 0:
			self.stride = self.width
		self.buffer = ffi.new("color_t[{}]".format(self.stride * self.height))

	def savePNG(self, f):
		p = png.PNG(f)
		success = p.writeHeader(self)
		success = success and p.writePixels(self)
		p.writeClose()
		return success
