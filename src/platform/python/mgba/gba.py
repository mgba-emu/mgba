# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from .arm import ARMCore
from .core import Core, needs_reset
from .tile import Sprite
from .memory import Memory
from . import create_callback


class GBA(Core):
    KEY_A = lib.GBA_KEY_A
    KEY_B = lib.GBA_KEY_B
    KEY_SELECT = lib.GBA_KEY_SELECT
    KEY_START = lib.GBA_KEY_START
    KEY_DOWN = lib.GBA_KEY_DOWN
    KEY_UP = lib.GBA_KEY_UP
    KEY_LEFT = lib.GBA_KEY_LEFT
    KEY_RIGHT = lib.GBA_KEY_RIGHT
    KEY_L = lib.GBA_KEY_L
    KEY_R = lib.GBA_KEY_R

    SIO_NORMAL_8 = lib.SIO_NORMAL_8
    SIO_NORMAL_32 = lib.SIO_NORMAL_32
    SIO_MULTI = lib.SIO_MULTI
    SIO_UART = lib.SIO_UART
    SIO_JOYBUS = lib.SIO_JOYBUS
    SIO_GPIO = lib.SIO_GPIO

    def __init__(self, native):
        super(GBA, self).__init__(native)
        self._native = ffi.cast("struct GBA*", native.board)
        self.sprites = GBAObjs(self)
        self.cpu = ARMCore(self._core.cpu)
        self.memory = None
        self._sio = set()

    @needs_reset
    def _init_cache(self, cache):
        lib.GBAVideoCacheInit(cache)
        lib.GBAVideoCacheAssociate(cache, ffi.addressof(self._native.video))

    def _deinit_cache(self, cache):
        lib.mCacheSetDeinit(cache)
        if self._was_reset:
            self._native.video.renderer.cache = ffi.NULL

    def _load(self):
        super(GBA, self)._load()
        self.memory = GBAMemory(self._core, self._native.memory.romSize)

    def attach_sio(self, link, mode=lib.SIO_MULTI):
        self._sio.add(mode)
        lib.GBASIOSetDriver(ffi.addressof(self._native.sio), link._native, mode)

    def __del__(self):
        for mode in self._sio:
            lib.GBASIOSetDriver(ffi.addressof(self._native.sio), ffi.NULL, mode)


create_callback("GBASIOPythonDriver", "init")
create_callback("GBASIOPythonDriver", "deinit")
create_callback("GBASIOPythonDriver", "load")
create_callback("GBASIOPythonDriver", "unload")
create_callback("GBASIOPythonDriver", "writeRegister")


class GBASIODriver(object):
    def __init__(self):
        super(GBASIODriver, self).__init__()

        self._handle = ffi.new_handle(self)
        self._native = ffi.gc(lib.GBASIOPythonDriverCreate(self._handle), lib.free)

    def init(self):
        return True

    def deinit(self):
        pass

    def load(self):
        return True

    def unload(self):
        return True

    def write_register(self, address, value):
        return value


class GBASIOJOYDriver(GBASIODriver):
    RESET = lib.JOY_RESET
    POLL = lib.JOY_POLL
    TRANS = lib.JOY_TRANS
    RECV = lib.JOY_RECV

    def __init__(self):
        super(GBASIOJOYDriver, self).__init__()

        self._native = ffi.gc(lib.GBASIOJOYPythonDriverCreate(self._handle), lib.free)

    def send_command(self, cmd, data):
        buffer = ffi.new('uint8_t[5]')
        try:
            buffer[0] = data[0]
            buffer[1] = data[1]
            buffer[2] = data[2]
            buffer[3] = data[3]
            buffer[4] = data[4]
        except IndexError:
            pass

        outlen = lib.GBASIOJOYSendCommand(self._native, cmd, buffer)
        if outlen > 0 and outlen <= 5:
            return bytes(buffer[0:outlen])
        return None


class GBAMemory(Memory):
    def __init__(self, core, romSize=lib.SIZE_CART0):
        super(GBAMemory, self).__init__(core, 0x100000000)

        self.bios = Memory(core, lib.GBA_SIZE_BIOS, lib.GBA_BASE_BIOS)
        self.wram = Memory(core, lib.GBA_SIZE_EWRAM, lib.GBA_BASE_EWRAM)
        self.iwram = Memory(core, lib.GBA_SIZE_IWRAM, lib.GBA_BASE_IWRAM)
        self.io = Memory(core, lib.GBA_SIZE_IO, lib.GBA_BASE_IO)  # pylint: disable=invalid-name
        self.palette = Memory(core, lib.GBA_SIZE_PALETTE_RAM, lib.GBA_BASE_PALETTE_RAM)
        self.vram = Memory(core, lib.GBA_SIZE_VRAM, lib.GBA_BASE_VRAM)
        self.oam = Memory(core, lib.GBA_SIZE_OAM, lib.GBA_BASE_OAM)
        self.cart0 = Memory(core, romSize, lib.BASE_CART0)
        self.cart1 = Memory(core, romSize, lib.BASE_CART1)
        self.cart2 = Memory(core, romSize, lib.BASE_CART2)
        self.cart = self.cart0
        self.rom = self.cart0
        self.sram = Memory(core, lib.GBA_SIZE_SRAM, lib.GBA_BASE_SRAM)


class GBASprite(Sprite):
    TILE_BASE = 0x800, 0x400
    PALETTE_BASE = 0x10, 1

    def __init__(self, obj):
        self._a = obj.a
        self._b = obj.b
        self._c = obj.c
        self.x = self._b & 0x1FF  # pylint: disable=invalid-name
        self.y = self._a & 0xFF  # pylint: disable=invalid-name
        self._shape = self._a >> 14
        self._size = self._b >> 14
        self._256_color = bool(self._a & 0x2000)
        self.width, self.height = lib.GBAVideoObjSizes[self._shape * 4 + self._size]
        self.tile = self._c & 0x3FF
        if self._256_color:
            self.palette_id = 0
            self.tile >>= 1
        else:
            self.palette_id = self._c >> 12


class GBAObjs:
    def __init__(self, core):
        self._core = core
        self._obj = core._native.video.oam.obj

    def __len__(self):
        return 128

    def __getitem__(self, index):
        if index >= len(self):
            raise IndexError()
        sprite = GBASprite(self._obj[index])
        tiles = self._core.tiles[3 if sprite._256_color else 2]
        map_1d = bool(self._core._native.memory.io[0] & 0x40)
        sprite.constitute(tiles, 0 if map_1d else 0x20)
        return sprite
