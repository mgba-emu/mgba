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
    return Core._init(core)

def findVF(vf):
    core = lib.mCoreFindVF(vf.handle)
    if core == ffi.NULL:
        return None
    return Core._init(core)

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

def needsReset(f):
    def wrapper(self, *args, **kwargs):
        if not self._wasReset:
            raise RuntimeError("Core must be reset first")
        return f(self, *args, **kwargs)
    return wrapper

class Core(object):
    if hasattr(lib, 'PLATFORM_GBA'):
        PLATFORM_GBA = lib.PLATFORM_GBA

    if hasattr(lib, 'PLATFORM_GB'):
        PLATFORM_GB = lib.PLATFORM_GB

    def __init__(self, native):
        self._core = native
        self._wasReset = False

    @cached_property
    def tiles(self):
        return tile.TileView(self)

    @classmethod
    def _init(cls, native):
        core = ffi.gc(native, native.deinit)
        success = bool(core.init(core))
        if not success:
            raise RuntimeError("Failed to initialize core")
        if hasattr(cls, 'PLATFORM_GBA') and core.platform(core) == cls.PLATFORM_GBA:
            from .gba import GBA
            return GBA(core)
        if hasattr(cls, 'PLATFORM_GB') and core.platform(core) == cls.PLATFORM_GB:
            from .gb import GB
            return GB(core)
        return Core(core)

    def _deinit(self):
        self._core.deinit(self._core)

    def loadFile(self, path):
        return bool(lib.mCoreLoadFile(self._core, path.encode('UTF-8')))

    def isROM(self, vf):
        return bool(self._core.isROM(vf.handle))

    def loadROM(self, vf):
        return bool(self._core.loadROM(self._core, vf.handle))

    def loadSave(self, vf):
        return bool(self._core.loadSave(self._core, vf.handle))

    def loadTemporarySave(self, vf):
        return bool(self._core.loadTemporarySave(self._core, vf.handle))

    def loadPatch(self, vf):
        return bool(self._core.loadPatch(self._core, vf.handle))

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
        self._wasReset = True

    @needsReset
    def runFrame(self):
        self._core.runFrame(self._core)

    @needsReset
    def runLoop(self):
        self._core.runLoop(self._core)

    @needsReset
    def step(self):
        self._core.step(self._core)

    @staticmethod
    def _keysToInt(*args, **kwargs):
        keys = 0
        if 'raw' in kwargs:
            keys = kwargs['raw']
        for key in args:
            keys |= 1 << key
        return keys

    def setKeys(self, *args, **kwargs):
        self._core.setKeys(self._core, self._keysToInt(*args, **kwargs))

    def addKeys(self, *args, **kwargs):
        self._core.addKeys(self._core, self._keysToInt(*args, **kwargs))

    def clearKeys(self, *args, **kwargs):
        self._core.clearKeys(self._core, self._keysToInt(*args, **kwargs))

    @needsReset
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
