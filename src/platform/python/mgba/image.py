# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import png

try:
    import PIL.Image as PImage
except ImportError:
    pass

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

    if 'PImage' in globals():
        def toPIL(self):
            return PImage.frombytes("RGBX", (self.width, self.height), ffi.buffer(self.buffer), "raw",
                                    "RGBX", self.stride * 4)

def u16ToU32(c):
    r = c & 0x1F
    g = (c >> 5) & 0x1F
    b = (c >> 10) & 0x1F
    a = (c >> 15) & 1
    abgr = r << 3
    abgr |= g << 11
    abgr |= b << 19
    abgr |= (a * 0xFF) << 24
    return abgr

def u32ToU16(c):
    r = (c >> 3) & 0x1F
    g = (c >> 11) & 0x1F
    b = (c >> 19) & 0x1F
    a = c >> 31
    abgr = r
    abgr |= g << 5
    abgr |= b << 10
    abgr |= a << 15
    return abgr

if ffi.sizeof("color_t") == 2:
    def colorToU16(c):
        return c

    colorToU32 = u16ToU32

    def u16ToColor(c):
        return c

    u32ToColor = u32ToU16
else:
    def colorToU32(c):
        return c

    colorToU16 = u32ToU16

    def u32ToColor(c):
        return c

    u16ToColor = u16ToU32
