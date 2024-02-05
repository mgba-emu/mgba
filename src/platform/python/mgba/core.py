# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from . import tile, audio
from cached_property import cached_property
from functools import wraps


def find(path):
    core = lib.mCoreFind(path.encode('UTF-8'))
    if core == ffi.NULL:
        return None
    return Core._init(core)


def find_vf(vfile):
    core = lib.mCoreFindVF(vfile.handle)
    if core == ffi.NULL:
        return None
    return Core._init(core)


def load_path(path):
    core = find(path)
    if not core or not core.load_file(path):
        return None
    return core


def load_vf(vfile):
    core = find_vf(vfile)
    if not core or not core.load_rom(vfile):
        return None
    return core


def needs_reset(func):
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        if not self._was_reset:
            raise RuntimeError("Core must be reset first")
        return func(self, *args, **kwargs)
    return wrapper


def protected(func):
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        if self._protected:
            raise RuntimeError("Core is protected")
        return func(self, *args, **kwargs)
    return wrapper


@ffi.def_extern()
def _mCorePythonCallbacksVideoFrameStarted(user):  # pylint: disable=invalid-name
    context = ffi.from_handle(user)
    context._video_frame_started()


@ffi.def_extern()
def _mCorePythonCallbacksVideoFrameEnded(user):  # pylint: disable=invalid-name
    context = ffi.from_handle(user)
    context._video_frame_ended()


@ffi.def_extern()
def _mCorePythonCallbacksCoreCrashed(user):  # pylint: disable=invalid-name
    context = ffi.from_handle(user)
    context._core_crashed()


@ffi.def_extern()
def _mCorePythonCallbacksSleep(user):  # pylint: disable=invalid-name
    context = ffi.from_handle(user)
    context._sleep()


@ffi.def_extern()
def _mCorePythonCallbacksKeysRead(user):  # pylint: disable=invalid-name
    context = ffi.from_handle(user)
    context._keys_read()


class CoreCallbacks(object):
    def __init__(self):
        self._handle = ffi.new_handle(self)
        self.video_frame_started = []
        self.video_frame_ended = []
        self.core_crashed = []
        self.sleep = []
        self.keys_read = []
        self.context = lib.mCorePythonCallbackCreate(self._handle)

    def _video_frame_started(self):
        for callback in self.video_frame_started:
            callback()

    def _video_frame_ended(self):
        for callback in self.video_frame_ended:
            callback()

    def _core_crashed(self):
        for callback in self.core_crashed:
            callback()

    def _sleep(self):
        for callback in self.sleep:
            callback()

    def _keys_read(self):
        for callback in self.keys_read:
            callback()


class Core(object):
    if hasattr(lib, 'mPLATFORM_GBA'):
        PLATFORM_GBA = lib.mPLATFORM_GBA

    if hasattr(lib, 'mPLATFORM_GB'):
        PLATFORM_GB = lib.mPLATFORM_GB

    def __init__(self, native):
        self._core = native
        self._was_reset = False
        self._protected = False
        self._callbacks = CoreCallbacks()
        self._core.addCoreCallbacks(self._core, self._callbacks.context)
        self.config = Config(ffi.addressof(native.config))

    def __del__(self):
        self._was_reset = False

    @cached_property
    def graphics_cache(self):
        if not self._was_reset:
            raise RuntimeError("Core must be reset first")
        return tile.CacheSet(self)

    @cached_property
    def tiles(self):
        tiles = []
        native_tiles = ffi.addressof(self.graphics_cache.cache.tiles)
        for i in range(lib.mTileCacheSetSize(native_tiles)):
            tiles.append(tile.TileView(lib.mTileCacheSetGetPointer(native_tiles, i)))
        return tiles

    @cached_property
    def maps(self):
        maps = []
        native_maps = ffi.addressof(self.graphics_cache.cache.maps)
        for i in range(lib.mMapCacheSetSize(native_maps)):
            maps.append(tile.MapView(lib.mMapCacheSetGetPointer(native_maps, i)))
        return maps

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
        self._was_reset = True

    @protected
    def load_file(self, path):
        return bool(lib.mCoreLoadFile(self._core, path.encode('UTF-8')))

    def is_rom(self, vfile):
        return bool(self._core.isROM(vfile.handle))

    @protected
    def load_rom(self, vfile):
        return bool(self._core.loadROM(self._core, vfile.handle))

    @protected
    def load_bios(self, vfile, id=0):
        res = bool(self._core.loadBIOS(self._core, vfile.handle, id))
        if res:
            vfile._claimed = True
        return res

    @protected
    def load_save(self, vfile):
        res = bool(self._core.loadSave(self._core, vfile.handle))
        if res:
            vfile._claimed = True
        return res

    @protected
    def load_temporary_save(self, vfile):
        return bool(self._core.loadTemporarySave(self._core, vfile.handle))

    @protected
    def load_patch(self, vfile):
        return bool(self._core.loadPatch(self._core, vfile.handle))

    @protected
    def load_config(self, config):
        lib.mCoreLoadForeignConfig(self._core, config._native)

    @protected
    def autoload_save(self):
        return bool(lib.mCoreAutoloadSave(self._core))

    @protected
    def autoload_patch(self):
        return bool(lib.mCoreAutoloadPatch(self._core))

    @protected
    def autoload_cheats(self):
        return bool(lib.mCoreAutoloadCheats(self._core))

    @property
    def platform(self):
        return self._core.platform(self._core)

    @protected
    def desired_video_dimensions(self):
        width = ffi.new("unsigned*")
        height = ffi.new("unsigned*")
        self._core.currentVideoSize(self._core, width, height)
        return width[0], height[0]

    @protected
    def set_video_buffer(self, image):
        self._core.setVideoBuffer(self._core, image.buffer, image.stride)

    @protected
    def set_audio_buffer_size(self, size):
        self._core.setAudioBufferSize(self._core, size)

    @property
    def audio_buffer_size(self):
        return self._core.getAudioBufferSize(self._core)

    @protected
    def get_audio_channels(self):
        return audio.StereoBuffer(self.get_audio_channel(0), self.get_audio_channel(1));

    @protected
    def get_audio_channel(self, channel):
        return audio.Buffer(self._core.getAudioChannel(self._core, channel), self.frequency)

    @protected
    def reset(self):
        self._core.reset(self._core)
        self._load()

    @needs_reset
    @protected
    def run_frame(self):
        self._core.runFrame(self._core)

    @needs_reset
    @protected
    def run_loop(self):
        self._core.runLoop(self._core)

    @needs_reset
    @protected
    def step(self):
        self._core.step(self._core)

    @needs_reset
    @protected
    def load_raw_state(self, state):
        if len(state) < self._core.stateSize(self._core):
            return False
        return self._core.loadState(self._core, state)

    @needs_reset
    @protected
    def save_raw_state(self):
        state = ffi.new('unsigned char[%i]' % self._core.stateSize(self._core))
        if self._core.saveState(self._core, state):
            return state
        return None

    @staticmethod
    def _keys_to_int(*args, **kwargs):
        keys = 0
        if 'raw' in kwargs:
            keys = kwargs['raw']
        for key in args:
            keys |= 1 << key
        return keys

    @protected
    def set_keys(self, *args, **kwargs):
        self._core.setKeys(self._core, self._keys_to_int(*args, **kwargs))

    @protected
    def add_keys(self, *args, **kwargs):
        self._core.addKeys(self._core, self._keys_to_int(*args, **kwargs))

    @protected
    def clear_keys(self, *args, **kwargs):
        self._core.clearKeys(self._core, self._keys_to_int(*args, **kwargs))

    @property
    @needs_reset
    def frame_counter(self):
        return self._core.frameCounter(self._core)

    @property
    def frame_cycles(self):
        return self._core.frameCycles(self._core)

    @property
    def frequency(self):
        return self._core.frequency(self._core)

    @property
    def game_title(self):
        title = ffi.new("char[16]")
        self._core.getGameTitle(self._core, title)
        return ffi.string(title, 16).decode("ascii")

    @property
    def game_code(self):
        code = ffi.new("char[12]")
        self._core.getGameCode(self._core, code)
        return ffi.string(code, 12).decode("ascii")

    def add_frame_callback(self, callback):
        self._callbacks.video_frame_ended.append(callback)

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

    def use_core(self):
        raise NotImplementedError

    @property
    def running(self):
        raise NotImplementedError

    @property
    def paused(self):
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
