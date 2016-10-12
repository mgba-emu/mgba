from _pylib import ffi, lib

class _ARMRegisters:
	def __init__(self, cpu):
		self._cpu = cpu

	def __getitem__(self, r):
		if r > lib.ARM_PC:
			raise IndexError("Register out of range")
		return int(self._cpu._native.gprs[r])

class ARMCore:
    def __init__(self, native):
    	self._native = ffi.cast("struct ARMCore*", native)
    	self.gprs = _ARMRegisters(self)

