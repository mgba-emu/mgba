# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from . import tile, createCallback
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

def protected(f):
    def wrapper(self, *args, **kwargs):
        if self._protected:
            raise RuntimeError("Core is protected")
        return f(self, *args, **kwargs)
    return wrapper

@ffi.def_extern()
def _mCorePythonCallbacksVideoFrameStarted(user):
    context = ffi.from_handle(user)
    context._videoFrameStarted()

@ffi.def_extern()
def _mCorePythonCallbacksVideoFrameEnded(user):
    context = ffi.from_handle(user)
    context._videoFrameEnded()

@ffi.def_extern()
def _mCorePythonCallbacksCoreCrashed(user):
    context = ffi.from_handle(user)
    context._coreCrashed()

@ffi.def_extern()
def _mCorePythonCallbacksSleep(user):
    context = ffi.from_handle(user)
    context._sleep()

class CoreCallbacks(object):
    def __init__(self):
        self._handle = ffi.new_handle(self)
        self.videoFrameStarted = []
        self.videoFrameEnded = []
        self.coreCrashed = []
        self.sleep = []
        self.context = lib.mCorePythonCallbackCreate(self._handle)

    def _videoFrameStarted(self):
        for cb in self.videoFrameStarted:
            cb()

    def _videoFrameEnded(self):
        for cb in self.videoFrameEnded:
            cb()

    def _coreCrashed(self):
        for cb in self.coreCrashed:
            cb()

    def _sleep(self):
        for cb in self.sleep:
            cb()

class Core(object):
    if hasattr(lib, 'PLATFORM_GBA'):
        PLATFORM_GBA = lib.PLATFORM_GBA

    if hasattr(lib, 'PLATFORM_GB'):
        PLATFORM_GB = lib.PLATFORM_GB

    def __init__(self, native):
        self._core = native
        self._wasReset = False
        self._protected = False
        self._callbacks = CoreCallbacks()
        self._core.addCoreCallbacks(self._core, self._callbacks.context)
        self.config = Config(ffi.addressof(native.config))

    def __del__(self):
        self._wasReset = False

    @cached_property
    def graphicsCache(self):
        if not self._wasReset:
            raise RuntimeError("Core must be reset first")
        return tile.CacheSet(self)

    @cached_property
    def tiles(self):
        t = []
        ts = ffi.addressof(self.graphicsCache.cache.tiles)
        for i in range(lib.mTileCacheSetSize(ts)):
            t.append(tile.TileView(lib.mTileCacheSetGetPointer(ts, i)))
        return t

    @cached_property
    def maps(self):
        m = []
        ms = ffi.addressof(self.graphicsCache.cache.maps)
        for i in range(lib.mMapCacheSetSize(ms)):
            m.append(tile.MapView(lib.mMapCacheSetGetPointer(ms, i)))
        return m

    @classmethod
    def _init(cls, native):
        core = ffi.gc(native, native.deinit)
        success = bool(core.init(core))
        lib.mCoreInitConfig(core, ffi.NULL)
        if not success:
            raise RuntimeError("Failed to initialize core")
        return cls._detect(core)

    @classmethod
    def _detect(cls, core):
        if hasattr(cls, 'PLATFORM_GBA') and core.platform(core) == cls.PLATFORM_GBA:
            from .gba import GBA
            return GBA(core)
        if hasattr(cls, 'PLATFORM_GB') and core.platform(core) == cls.PLATFORM_GB:
            from .gb import GB
            return GB(core)
        return Core(core)

    def _load(self):
        self._wasReset = True

    def loadFile(self, path):
        return bool(lib.mCoreLoadFile(self._core, path.encode('UTF-8')))

    def isROM(self, vf):
        return bool(self._core.isROM(vf.handle))

    def loadROM(self, vf):
        return bool(self._core.loadROM(self._core, vf.handle))

    def loadBIOS(self, vf, id=0):
        return bool(self._core.loadBIOS(self._core, vf.handle, id))

    def loadSave(self, vf):
        return bool(self._core.loadSave(self._core, vf.handle))

    def loadTemporarySave(self, vf):
        return bool(self._core.loadTemporarySave(self._core, vf.handle))

    def loadPatch(self, vf):
        return bool(self._core.loadPatch(self._core, vf.handle))

    def loadConfig(self, config):
        lib.mCoreLoadForeignConfig(self._core, config._native)

    def autoloadSave(self):
        return bool(lib.mCoreAutoloadSave(self._core))

    def autoloadPatch(self):
        return bool(lib.mCoreAutoloadPatch(self._core))

    def autoloadCheats(self):
        return bool(lib.mCoreAutoloadCheats(self._core))

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
        self._load()

    @needsReset
    @protected
    def runFrame(self):
        self._core.runFrame(self._core)

    @needsReset
    @protected
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

    @property
    @needsReset
    def frameCounter(self):
        return self._core.frameCounter(self._core)

    @property
    def frameCycles(self):
        return self._core.frameCycles(self._core)

    @property
    def frequency(self):
        return self._core.frequency(self._core)

    @property
    def gameTitle(self):
        title = ffi.new("char[16]")
        self._core.getGameTitle(self._core, title)
        return ffi.string(title, 16).decode("ascii")

    @property
    def gameCode(self):
        code = ffi.new("char[12]")
        self._core.getGameCode(self._core, code)
        return ffi.string(code, 12).decode("ascii")

    def addFrameCallback(self, cb):
        self._callbacks.videoFrameEnded.append(cb)

    @property
    def crc32(self):
        return self._native.romCrc32

class ICoreOwner(object):
    def claim(self):
        raise NotImplementedError

    def release(self):
        raise NotImplementedError

    def __enter__(self):
        self.core = self.claim()
        self.core._protected = True
        return self.core

    def __exit__(self, type, value, traceback):
        self.core._protected = False
        self.release()

class IRunner(object):
    def pause(self):
        raise NotImplementedError

    def unpause(self):
        raise NotImplementedError

    def useCore(self):
        raise NotImplementedError

    def isRunning(self):
        raise NotImplementedError

    def isPaused(self):
        raise NotImplementedError

class Config(object):
    def __init__(self, native=None, port=None, defaults={}):
        if not native:
            self._port = ffi.NULL
            if port:
                self._port = ffi.new("char[]", port.encode("UTF-8"))
            native = ffi.gc(ffi.new("struct mCoreConfig*"), lib.mCoreConfigDeinit)
            lib.mCoreConfigInit(native, self._port)
        self._native = native
        for key, value in defaults.items():
            if isinstance(value, bool):
                value = int(value)
            lib.mCoreConfigSetDefaultValue(self._native, ffi.new("char[]", key.encode("UTF-8")), ffi.new("char[]", str(value).encode("UTF-8")))

    def __getitem__(self, key):
        string = lib.mCoreConfigGetValue(self._native, ffi.new("char[]", key.encode("UTF-8")))
        if not string:
            return None
        return ffi.string(string)

    def __setitem__(self, key, value):
        if isinstance(value, bool):
            value = int(value)
        lib.mCoreConfigSetValue(self._native, ffi.new("char[]", key.encode("UTF-8")), ffi.new("char[]", str(value).encode("UTF-8")))
