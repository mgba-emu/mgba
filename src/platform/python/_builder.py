import cffi
import os, os.path
import shlex
import subprocess
import sys

ffi = cffi.FFI()
pydir = os.path.dirname(os.path.abspath(__file__))
srcdir = os.path.join(pydir, "..", "..")
incdir = os.path.join(pydir, "..", "..", "..", "include")
bindir = os.environ.get("BINDIR", os.path.join(os.getcwd(), ".."))

cpp = shlex.split(os.environ.get("CPP", "cc -E"))
cppflags = shlex.split(os.environ.get("CPPFLAGS", ""))
if __name__ == "__main__":
    cppflags.extend(sys.argv[1:])
cppflags.extend(["-I" + incdir, "-I" + srcdir, "-I" + bindir])

ffi.set_source("mgba._pylib", """
#include "flags.h"
#include <mgba-util/common.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/core/mem-search.h>
#include <mgba/core/tile-cache.h>
#include <mgba/core/version.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gba/renderers/tile-cache.h>
#include <mgba/internal/lr35902/lr35902.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/renderers/tile-cache.h>
#include <mgba-util/png-io.h>
#include <mgba-util/vfs.h>

#define PYEXPORT
#include "platform/python/log.h"
#include "platform/python/sio.h"
#include "platform/python/vfs-py.h"
#undef PYEXPORT
""", include_dirs=[incdir, srcdir],
     extra_compile_args=cppflags,
     libraries=["mgba"],
     library_dirs=[bindir],
     sources=[os.path.join(pydir, path) for path in ["vfs-py.c", "log.c", "sio.c"]])

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
