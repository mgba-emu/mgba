# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from . import vfs

MODE_RGB = 0
MODE_RGBA = 1
MODE_INDEX = 2


class PNG:
    def __init__(self, f, mode=MODE_RGB):
        self._vfile = vfs.open(f)
        self._png = None
        self._info = None
        self.mode = mode

    def write_header(self, image):
        self._png = lib.PNGWriteOpen(self._vfile.handle)
        if self.mode == MODE_RGB:
            self._info = lib.PNGWriteHeader(self._png, image.width, image.height, lib.mCOLOR_XBGR8)
        if self.mode == MODE_RGBA:
            self._info = lib.PNGWriteHeader(self._png, image.width, image.height, lib.mCOLOR_ABGR8)
        return self._info != ffi.NULL

    def write_pixels(self, image):
        if self.mode == MODE_RGB:
            return lib.PNGWritePixels(self._png, image.width, image.height, image.stride, image.buffer, lib.mCOLOR_XBGR8)
        if self.mode == MODE_RGBA:
            return lib.PNGWritePixels(self._png, image.width, image.height, image.stride, image.buffer, lib.mCOLOR_ABGR8)
        return False

    def write_close(self):
        lib.PNGWriteClose(self._png, self._info)
        self._png = None
        self._info = None
