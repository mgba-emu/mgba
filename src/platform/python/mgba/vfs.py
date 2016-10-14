from _pylib import ffi, lib
import mmap
import os

@ffi.def_extern()
def _vfpClose(vf):
	vfp = ffi.cast("struct VFilePy*", vf)
	ffi.from_handle(vfp.fileobj).close()

@ffi.def_extern()
def _vfpSeek(vf, offset, whence):
	vfp = ffi.cast("struct VFilePy*", vf)
	f = ffi.from_handle(vfp.fileobj)
	f.seek(offset, whence)
	return f.tell()

@ffi.def_extern()
def _vfpRead(vf, buffer, size):
	vfp = ffi.cast("struct VFilePy*", vf)
	pybuf = ffi.buffer(buffer, size)
	return ffi.from_handle(vfp.fileobj).readinto(pybuf)

@ffi.def_extern()
def _vfpWrite(vf, buffer, size):
	vfp = ffi.cast("struct VFilePy*", vf)
	pybuf = ffi.buffer(buffer, size)
	return ffi.from_handle(vfp.fileobj).write(pybuf)

@ffi.def_extern()
def _vfpMap(vf, size, flags):
	pass

@ffi.def_extern()
def _vfpUnmap(vf, memory, size):
	pass

@ffi.def_extern()
def _vfpTruncate(vf, size):
	vfp = ffi.cast("struct VFilePy*", vf)
	ffi.from_handle(vfp.fileobj).truncate(size)

@ffi.def_extern()
def _vfpSize(vf):
	vfp = ffi.cast("struct VFilePy*", vf)
	f = ffi.from_handle(vfp.fileobj)
	pos = f.tell()
	f.seek(0, os.SEEK_END)
	size = f.tell()
	f.seek(pos, os.SEEK_SET)
	return size

@ffi.def_extern()
def _vfpSync(vf, buffer, size):
	vfp = ffi.cast("struct VFilePy*", vf)
	f = ffi.from_handle(vfp.fileobj)
	if buffer and size:
		pos = f.tell()
		f.seek(0, os.SEEK_SET)
		_vfpWrite(vf, buffer, size)
		f.seek(pos, os.SEEK_SET)
	f.flush()
	os.fsync()
	return True

def open(f):
	handle = ffi.new_handle(f)
	vf = VFile(lib.VFileFromPython(handle))
	# Prevent garbage collection
	vf._fileobj = f
	vf._handle = handle
	return vf

def openPath(path, mode="r"):
	flags = 0
	if mode.startswith("r"):
		flags |= os.O_RDONLY
	elif mode.startswith("w"):
		flags |= os.O_WRONLY | os.O_CREAT | os.O_TRUNC
	elif mode.startswith("a"):
		flags |= os.O_WRONLY | os.O_CREAT | os.O_APPEND
	else:
		return None

	if "+" in mode[1:]:
		flags |= os.O_RDWR
	if "x" in mode[1:]:
		flags |= os.O_EXCL

	return VFile(lib.VFileOpen(path.encode("UTF-8"), flags))

class VFile:
	def __init__(self, vf):
		self._vf = vf

	def handle(self):
		return self._vf

	def close(self):
		return self._vf.close(self._vf)

	def seek(self, offset, whence):
		return self._vf.seek(self._vf, offset, whence)

	def read(self, buffer, size):
		return self._vf.read(self._vf, buffer, size)

	def readline(self, buffer, size):
		return self._vf.readline(self._vf, buffer, size)

	def write(self, buffer, size):
		return self._vf.write(self._vf, buffer, size)

	def map(self, size, flags):
		return self._vf.map(self._vf, size, flags)

	def unmap(self, memory, size):
		self._vf.unmap(self._vf, memory, size)

	def truncate(self, size):
		self._vf.truncate(self._vf, size)

	def size(self):
		return self._vf.size(self._vf)

	def sync(self, buffer, size):
		return self._vf.sync(self._vf, buffer, size)
