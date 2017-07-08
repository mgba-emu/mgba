# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib
from .core import IRunner, ICoreOwner, Core

class DebuggerCoreOwner(ICoreOwner):
    def __init__(self, debugger):
        self.debugger = debugger
        self.wasPaused = False

    def claim(self):
        if self.debugger.isRunning():
            self.wasPaused = True
            self.debugger.pause()
        return self.debugger._core

    def release(self):
        if self.wasPaused:
            self.debugger.unpause()

class NativeDebugger(IRunner):
    WATCHPOINT_WRITE = lib.WATCHPOINT_WRITE
    WATCHPOINT_READ = lib.WATCHPOINT_READ
    WATCHPOINT_RW = lib.WATCHPOINT_RW

    def __init__(self, native):
        self._native = native
        self._core = Core._detect(native.core)
        self._core._wasReset = True

    def pause(self):
        lib.mDebuggerEnter(self._native, lib.DEBUGGER_ENTER_MANUAL, ffi.NULL)

    def unpause(self):
        self._native.state = lib.DEBUGGER_RUNNING

    def isRunning(self):
        return self._native.state == lib.DEBUGGER_RUNNING

    def isPaused(self):
        return self._native.state in (lib.DEBUGGER_PAUSED, lib.DEBUGGER_CUSTOM)

    def useCore(self):
        return DebuggerCoreOwner(self)

    def setBreakpoint(self, address):
        if not self._native.platform.setBreakpoint:
            raise RuntimeError("Platform does not support breakpoints")
        self._native.platform.setBreakpoint(self._native.platform, address)

    def clearBreakpoint(self, address):
        if not self._native.platform.setBreakpoint:
            raise RuntimeError("Platform does not support breakpoints")
        self._native.platform.clearBreakpoint(self._native.platform, address)

    def setWatchpoint(self, address):
        if not self._native.platform.setWatchpoint:
            raise RuntimeError("Platform does not support watchpoints")
        self._native.platform.setWatchpoint(self._native.platform, address)

    def clearWatchpoint(self, address):
        if not self._native.platform.clearWatchpoint:
            raise RuntimeError("Platform does not support watchpoints")
        self._native.platform.clearWatchpoint(self._native.platform, address)
