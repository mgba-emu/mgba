# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from .core import IRunner, ICoreOwner, Core
import sys


class DebuggerCoreOwner(ICoreOwner):
    def __init__(self, debugger):
        self.debugger = debugger
        self.was_paused = False

    def claim(self):
        if self.debugger.isRunning():
            self.was_paused = True
            self.debugger.pause()
        return self.debugger._core

    def release(self):
        if self.was_paused:
            self.debugger.unpause()


class NativeDebugger(IRunner):
    WATCHPOINT_WRITE = lib.WATCHPOINT_WRITE
    WATCHPOINT_READ = lib.WATCHPOINT_READ
    WATCHPOINT_RW = lib.WATCHPOINT_RW

    BREAKPOINT_HARDWARE = lib.BREAKPOINT_HARDWARE
    BREAKPOINT_SOFTWARE = lib.BREAKPOINT_SOFTWARE

    ENTER_MANUAL = lib.DEBUGGER_ENTER_MANUAL
    ENTER_ATTACHED = lib.DEBUGGER_ENTER_ATTACHED
    ENTER_BREAKPOINT = lib.DEBUGGER_ENTER_BREAKPOINT
    ENTER_WATCHPOINT = lib.DEBUGGER_ENTER_WATCHPOINT
    ENTER_ILLEGAL_OP = lib.DEBUGGER_ENTER_ILLEGAL_OP

    def __init__(self, native):
        self._native = native
        self._cbs = []
        self._core = Core._detect(native.core)
        self._core._load()

    def pause(self):
        lib.mDebuggerEnter(self._native, lib.DEBUGGER_ENTER_MANUAL, ffi.NULL)

    def unpause(self):
        self._native.state = lib.DEBUGGER_RUNNING

    @property
    def running(self):
        return self._native.state == lib.DEBUGGER_RUNNING

    @property
    def paused(self):
        return self._native.state in (lib.DEBUGGER_PAUSED, lib.DEBUGGER_CALLBACK)

    def use_core(self):
        return DebuggerCoreOwner(self)

    def set_breakpoint(self, address):
        if not self._native.platform.setBreakpoint:
            raise RuntimeError("Platform does not support breakpoints")
        self._native.platform.setBreakpoint(self._native.platform, address)

    def clear_breakpoint(self, address):
        if not self._native.platform.setBreakpoint:
            raise RuntimeError("Platform does not support breakpoints")
        self._native.platform.clearBreakpoint(self._native.platform, address)

    def set_watchpoint(self, address):
        if not self._native.platform.setWatchpoint:
            raise RuntimeError("Platform does not support watchpoints")
        self._native.platform.setWatchpoint(self._native.platform, address)

    def clear_watchpoint(self, address):
        if not self._native.platform.clearWatchpoint:
            raise RuntimeError("Platform does not support watchpoints")
        self._native.platform.clearWatchpoint(self._native.platform, address)

    def add_callback(self, callback):
        self._cbs.append(callback)


class CLIBackend(object):
    def __init__(self, backend):
        self.backend = backend

    def write(self, string):
        self.backend.printf(string)


class CLIDebugger(NativeDebugger):
    def __init__(self, native):
        super(CLIDebugger, self).__init__(native)
        self._cli = ffi.cast("struct CLIDebugger*", native)

    def printf(self, message, *args, **kwargs):
        message = message.format(*args, **kwargs)
        self._cli.backend.printf(ffi.new("char []", b"%s"), ffi.new("char []", message.encode('utf-8')))

    def install_print(self):
        sys.stdout = CLIBackend(self)
