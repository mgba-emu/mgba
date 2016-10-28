import cffi
import os, os.path
import shlex
import subprocess
import sys

ffi = cffi.FFI()
pydir = os.path.dirname(os.path.abspath(__file__))
srcdir = os.path.join(pydir, "..", "..")
bindir = os.environ.get("BINDIR", os.path.join(os.getcwd(), ".."))

cpp = shlex.split(os.environ.get("CPP", "cc -E"))
cppflags = shlex.split(os.environ.get("CPPFLAGS", ""))
if __name__ == "__main__":
    cppflags.extend(sys.argv[1:])
cppflags.extend(["-I" + srcdir, "-I" + bindir])

ffi.set_source("mgba._pylib", """
#include "flags.h"
#include "util/common.h"
#include "core/core.h"
#include "core/log.h"
#include "core/tile-cache.h"
#include "arm/arm.h"
#include "gba/gba.h"
#include "gba/input.h"
#include "gba/renderers/tile-cache.h"
#include "lr35902/lr35902.h"
#include "gb/gb.h"
#include "gb/renderers/tile-cache.h"
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
""", include_dirs=[srcdir],
     extra_compile_args=cppflags,
     libraries=["mgba"],
     library_dirs=[bindir],
     sources=[os.path.join(pydir, path) for path in ["vfs-py.c", "log.c"]])

preprocessed = subprocess.check_output(cpp + ["-fno-inline", "-P"] + cppflags + [os.path.join(pydir, "_builder.h")], universal_newlines=True)

lines = []
for line in preprocessed.splitlines():
    line = line.strip()
    if line.startswith('#'):
        continue
    lines.append(line)
ffi.cdef('\n'.join(lines))

if __name__ == "__main__":
    ffi.compile()
