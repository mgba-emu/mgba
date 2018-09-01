# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module
from . import create_callback

create_callback("mLoggerPy", "log", "_pyLog")


def install_default(logger):
    Logger.install_default(logger)


def silence():
    Logger.install_default(NullLogger())


class Logger(object):
    FATAL = lib.mLOG_FATAL
    DEBUG = lib.mLOG_DEBUG
    INFO = lib.mLOG_INFO
    WARN = lib.mLOG_WARN
    ERROR = lib.mLOG_ERROR
    STUB = lib.mLOG_STUB
    GAME_ERROR = lib.mLOG_GAME_ERROR

    _DEFAULT_LOGGER = None

    def __init__(self):
        self._handle = ffi.new_handle(self)
        self._native = ffi.gc(lib.mLoggerPythonCreate(self._handle), lib.free)

    @staticmethod
    def category_name(category):
        return ffi.string(lib.mLogCategoryName(category)).decode('UTF-8')

    @classmethod
    def install_default(cls, logger):
        cls._DEFAULT_LOGGER = logger
        lib.mLogSetDefaultLogger(logger._native)

    def log(self, category, level, message):
        print("{}: {}".format(self.category_name(category), message))


class NullLogger(Logger):
    def log(self, category, level, message):
        pass
