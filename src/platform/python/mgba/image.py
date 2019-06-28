# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi  # pylint: disable=no-name-in-module
from . import png

try:
    import PIL.Image as PImage
except ImportError:
    pass


class Image:
    def __init__(self, width, height, stride=0, alpha=False):
        self.width = width
        self.height = height
        self.stride = stride
        self.alpha = alpha
        self.constitute()

    def constitute(self):
        if self.stride <= 0:
            self.stride = self.width
        self.buffer = ffi.new("color_t[{}]".format(self.stride * self.height))

    def save_png(self, fileobj):
        png_file = png.PNG(fileobj, mode=png.MODE_RGBA if self.alpha else png.MODE_RGB)
        success = png_file.write_header(self)
        success = success and png_file.write_pixels(self)
        png_file.write_close()
        return success

    if 'PImage' in globals():
        def to_pil(self):
            colorspace = "RGBA" if self.alpha else "RGBX"
            return PImage.frombytes(colorspace, (self.width, self.height), ffi.buffer(self.buffer), "raw",
                                    colorspace, self.stride * 4)


def u16_to_u32(color):
    # pylint: disable=invalid-name
    r = color & 0x1F
    g = (color >> 5) & 0x1F
    b = (color >> 10) & 0x1F
    a = (color >> 15) & 1
    abgr = r << 3
    abgr |= g << 11
    abgr |= b << 19
    abgr |= (a * 0xFF) << 24
    return abgr


def u32_to_u16(color):
    # pylint: disable=invalid-name
    r = (color >> 3) & 0x1F
    g = (color >> 11) & 0x1F
    b = (color >> 19) & 0x1F
    a = color >> 31
    abgr = r
    abgr |= g << 5
    abgr |= b << 10
    abgr |= a << 15
    return abgr


if ffi.sizeof("color_t") == 2:
    def color_to_u16(color):
        return color

    color_to_u32 = u16_to_u32  # pylint: disable=invalid-name

    def u16_to_color(color):
        return color

    u32_to_color = u32_to_u16  # pylint: disable=invalid-name
else:
    def color_to_u32(color):
        return color

    color_to_u16 = u32_to_u16  # pylint: disable=invalid-name

    def u32_to_color(color):
        return color

    u16_to_color = u16_to_u32  # pylint: disable=invalid-name
