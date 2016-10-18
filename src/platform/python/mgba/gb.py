# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import core, lr35902

class GB:
    def __init__(self, native):
        super(GB, self).__init__(native)
        self._native = ffi.cast("struct GB*", native.board)
        self.cpu = lr35902.LR35902Core(self._core.cpu)

    def _initTileCache(self, cache):
        lib.GBVideoTileCacheInit(cache)
        lib.GBVideoTileCacheAssociate(cache, ffi.addressof(self._native.video))

    def _deinitTileCache(self, cache):
        self._native.video.renderer.cache = ffi.NULL
        lib.mTileCacheDeinit(cache)
