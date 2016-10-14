from _pylib import ffi, lib

class GBA:
    def __init__(self, native):
        self._native = ffi.cast("struct GBA*", native)
