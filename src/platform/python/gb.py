from _pylib import ffi, lib

class GB:
    def __init__(self, native):
    	self._native = ffi.cast("struct GB*", native)
