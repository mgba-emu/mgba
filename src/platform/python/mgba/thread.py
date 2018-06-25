# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from .core import IRunner, ICoreOwner, Core


class ThreadCoreOwner(ICoreOwner):
    def __init__(self, thread):
        self.thread = thread

    def claim(self):
        if not self.thread.isRunning():
            raise ValueError
        lib.mCoreThreadInterrupt(self.thread._native)
        return self.thread._core

    def release(self):
        lib.mCoreThreadContinue(self.thread._native)


class Thread(IRunner):
    def __init__(self, native=None):
        if native:
            self._native = native
            self._core = Core(native.core)
            self._core._was_reset = lib.mCoreThreadHasStarted(self._native)
        else:
            self._native = ffi.new("struct mCoreThread*")

    def start(self, core):
        if lib.mCoreThreadHasStarted(self._native):
            raise ValueError
        self._core = core
        self._native.core = core._core
        lib.mCoreThreadStart(self._native)
        self._core._was_reset = lib.mCoreThreadHasStarted(self._native)

    def end(self):
        if not lib.mCoreThreadHasStarted(self._native):
            raise ValueError
        lib.mCoreThreadEnd(self._native)
        lib.mCoreThreadJoin(self._native)

    def pause(self):
        lib.mCoreThreadPause(self._native)

    def unpause(self):
        lib.mCoreThreadUnpause(self._native)

    @property
    def running(self):
        return bool(lib.mCoreThreadIsActive(self._native))

    @property
    def paused(self):
        return bool(lib.mCoreThreadIsPaused(self._native))

    def use_core(self):
        return ThreadCoreOwner(self)
