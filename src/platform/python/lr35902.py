from _pylib import ffi, lib

class LR35902Core:
    def __init__(self, native):
    	self._native = ffi.cast("struct LR35902*", native)
