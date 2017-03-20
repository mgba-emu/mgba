# Copyright (c) 2013-2016 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib

@ffi.def_extern()
def _pyLog(logger, category, level, message):
	l = ffi.cast("struct mLoggerPy*", logger)
	ffi.from_handle(l.pyobj).log(category, level, ffi.string(message).decode('UTF-8'))

def installDefault(logger):
	lib.mLogSetDefaultLogger(logger._native)

class Logger(object):
	FATAL = lib.mLOG_FATAL
	DEBUG = lib.mLOG_DEBUG
	INFO = lib.mLOG_INFO
	WARN = lib.mLOG_WARN
	ERROR = lib.mLOG_ERROR
	STUB = lib.mLOG_STUB
	GAME_ERROR = lib.mLOG_GAME_ERROR

	def __init__(self):
		self._handle = ffi.new_handle(self)
		self._native = ffi.gc(lib.mLoggerPythonCreate(self._handle), lib.free)

	@staticmethod
	def categoryName(category):
		return ffi.string(lib.mLogCategoryName(category)).decode('UTF-8')

	def log(self, category, level, message):
		print("{}: {}".format(self.categoryName(category), message))

class NullLogger(Logger):
	def log(self, category, level, message):
		pass
