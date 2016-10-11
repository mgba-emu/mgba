import cffi
import os.path
import subprocess
import sys

ffi = cffi.FFI()
src = os.path.join(os.path.dirname(__file__), "..", "..")

ffi.set_source("mgba._pylib", """
#include "util/common.h"
#include "core/core.h"
""", include_dirs=[src],
     extra_compile_args=sys.argv[1:],
     libraries=["mgba"],
     library_dirs=[os.path.join(os.getcwd(), "..")])

with open(os.path.join(os.getcwd(), "_builder.h")) as core:
    ffi.cdef(core.read())

ffi.compile()