# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import tile
from cached_property import cached_property

def find(path):
    core = lib.mCoreFind(path.encode('UTF-8'))
    if core == ffi.NULL:
        return None
    return Core(core)

def findVF(vf):
    core = lib.mCoreFindVF(vf.handle)
    if core == ffi.NULL:
        return None
    return Core(core)

def loadPath(path):
    core = find(path)
    if not core or not core.loadFile(path):
        return None
    return core

def loadVF(vf):
    core = findVF(vf)
    if not core or not core.loadROM(vf):
        return None
    return core

class Core:
    def __init__(self, native):
        self._core = ffi.gc(native, native.deinit)
        success = bool(self._core.init(self._core))
        if not success:
            raise RuntimeError("Failed to initialize core")

        if hasattr(self, 'PLATFORM_GBA') and self.platform() == self.PLATFORM_GBA:
            self.cpu = ARMCore(self._core.cpu)
            self.board = GBA(self._core.board)
        if hasattr(self, 'PLATFORM_GB') and self.platform() == self.PLATFORM_GB:
            self.cpu = LR35902Core(self._core.cpu)
            self.board = GB(self._core.board)

    @cached_property
    def tiles(self):
        return tile.TileView(self)

    def _deinit(self):
        self._core.deinit(self._core)

    def loadFile(self, path):
        return bool(lib.mCoreLoadFile(self._core, path.encode('UTF-8')))

    def isROM(self, vf):
        return bool(self._core.isROM(vf.handle))

    def loadROM(self, vf):
        return bool(self._core.loadROM(self._core, vf.handle))

    def autoloadSave(self):
        return bool(lib.mCoreAutoloadSave(self._core))

    def autoloadPatch(self):
        return bool(lib.mCoreAutoloadPatch(self._core))

    def platform(self):
        return self._core.platform(self._core)

    def desiredVideoDimensions(self):
        width = ffi.new("unsigned*")
        height = ffi.new("unsigned*")
        self._core.desiredVideoDimensions(self._core, width, height)
        return width[0], height[0]

    def setVideoBuffer(self, image):
        self._core.setVideoBuffer(self._core, image.buffer, image.stride)

    def reset(self):
        self._core.reset(self._core)

    def runFrame(self):
        self._core.runFrame(self._core)

    def runLoop(self):
        self._core.runLoop(self._core)

    def step(self):
        self._core.step(self._core)

    def frameCounter(self):
        return self._core.frameCounter(self._core)

    def frameCycles(self):
        return self._core.frameCycles(self._core)

    def frequency(self):
        return self._core.frequency(self._core)

    def getGameTitle(self):
        title = ffi.new("char[16]")
        self._core.getGameTitle(self._core, title)
        return ffi.string(title, 16).decode("ascii")

    def getGameCode(self):
        code = ffi.new("char[12]")
        self._core.getGameCode(self._core, code)
        return ffi.string(code, 12).decode("ascii")

if hasattr(lib, 'PLATFORM_GBA'):
    from .gba import GBA
    from .arm import ARMCore
    Core.PLATFORM_GBA = lib.PLATFORM_GBA

if hasattr(lib, 'PLATFORM_GB'):
    from .gb import GB
    from .lr35902 import LR35902Core
    Core.PLATFORM_GB = lib.PLATFORM_GB
