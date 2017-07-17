# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from .arm import ARMCore
from .core import Core

class DS(Core):
    KEY_A = lib.DS_KEY_A
    KEY_B = lib.DS_KEY_B
    KEY_SELECT = lib.DS_KEY_SELECT
    KEY_START = lib.DS_KEY_START
    KEY_DOWN = lib.DS_KEY_DOWN
    KEY_UP = lib.DS_KEY_UP
    KEY_LEFT = lib.DS_KEY_LEFT
    KEY_RIGHT = lib.DS_KEY_RIGHT
    KEY_L = lib.DS_KEY_L
    KEY_R = lib.DS_KEY_R
    KEY_X = lib.DS_KEY_X
    KEY_Y = lib.DS_KEY_Y

    def __init__(self, native):
        super(DS, self).__init__(native)
        self._native = ffi.cast("struct DS*", native.board)
        self.arm7 = ARMCore(self._native.ds7.cpu)
        self.arm9 = ARMCore(self._native.ds9.cpu)
