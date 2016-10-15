import cffi
import os.path
import subprocess
import sys

ffi = cffi.FFI()
src = os.path.join(os.path.dirname(__file__), "..", "..")

ffi.set_source("mgba._pylib", """
#include "util/common.h"
#include "core/core.h"
#include "core/log.h"
#include "arm/arm.h"
#include "gba/gba.h"
#include "lr35902/lr35902.h"
#include "gb/gb.h"
#include "util/png-io.h"
#include "util/vfs.h"

struct VFile* VFileFromPython(void* fileobj);

struct VFilePy {
    struct VFile d;
    void* fileobj;
};

struct mLogger* mLoggerPythonCreate(void* pyobj);

struct mLoggerPy {
    struct mLogger d;
    void* pyobj;
};
""", include_dirs=[src],
     extra_compile_args=sys.argv[1:],
     libraries=["mgba"],
     library_dirs=[os.path.join(os.getcwd(), "..")],
     sources=[os.path.join(os.path.dirname(__file__), path) for path in ["vfs-py.c", "log.c"]])

with open(os.path.join(os.getcwd(), "_builder.h")) as core:
    lines = []
    for line in core:
        line = line.strip()
        if line.startswith('#'):
            continue
        lines.append(line)
    ffi.cdef('\n'.join(lines))

ffi.compile()