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
libdir = os.environ.get("LIBDIR")

cpp = shlex.split(os.environ.get("CPP", "cc -E"))
cppflags = shlex.split(os.environ.get("CPPFLAGS", ""))
cppflags.extend(["-I" + incdir, "-I" + srcdir, "-I" + bindir])

ffi.set_source("mgba._pylib", """
#define static
#define inline
#define MGBA_EXPORT
#include <mgba/flags.h>
#define OPAQUE_THREADING
#include <mgba/core/blip_buf.h>
#include <mgba/core/cache-set.h>
#include <mgba-util/common.h>
#include <mgba/core/core.h>
#include <mgba/core/map-cache.h>
#include <mgba/core/log.h>
#include <mgba/core/mem-search.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>
#include <mgba/debugger/debugger.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/debugger/cli-debugger.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gba/renderers/cache-set.h>
#include <mgba/internal/sm83/sm83.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/renderers/cache-set.h>
#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>

#define PYEXPORT
#include "platform/python/core.h"
#include "platform/python/log.h"
#include "platform/python/sio.h"
#include "platform/python/vfs-py.h"
#undef PYEXPORT
""", include_dirs=[incdir, srcdir],
     extra_compile_args=cppflags,
     libraries=["mgba"],
     library_dirs=[bindir],
     runtime_library_dirs=[libdir],
     sources=[os.path.join(pydir, path) for path in ["vfs-py.c", "core.c", "log.c", "sio.c"]])

preprocessed = subprocess.check_output(cpp + ["-fno-inline", "-P"] + cppflags + [os.path.join(pydir, "_builder.h")], universal_newlines=True)

lines = []
for line in preprocessed.splitlines():
    line = line.strip()
    if line.startswith('#'):
        continue
    lines.append(line)
ffi.cdef('\n'.join(lines))

preprocessed = subprocess.check_output(cpp + ["-fno-inline", "-P"] + cppflags + [os.path.join(pydir, "lib.h")], universal_newlines=True)

lines = []
for line in preprocessed.splitlines():
    line = line.strip()
    if line.startswith('#'):
        continue
    lines.append(line)
ffi.embedding_api('\n'.join(lines))

ffi.embedding_init_code("""
    import os, os.path
    from mgba._pylib import ffi, lib
    symbols = {}
    globalSyms = {
        'symbols': symbols
    }
    pendingCode = []

    @ffi.def_extern()
    def mPythonSetDebugger(debugger):
        from mgba.debugger import NativeDebugger, CLIDebugger
        oldDebugger = globalSyms.get('debugger')
        if oldDebugger and oldDebugger._native == debugger:
            return
        if oldDebugger and not debugger:
            del globalSyms['debugger']
            return
        if debugger.type == lib.DEBUGGER_CLI:
            debugger = CLIDebugger(debugger)
        else:
            debugger = NativeDebugger(debugger)
        globalSyms['debugger'] = debugger

    @ffi.def_extern()
    def mPythonLoadScript(name, vf):
        from mgba.vfs import VFile
        vf = VFile(vf)
        name = ffi.string(name)
        source = vf.read_all().decode('utf-8')
        try:
            code = compile(source, name, 'exec')
            pendingCode.append(code)
        except:
            return False
        return True

    @ffi.def_extern()
    def mPythonRunPending():
        global pendingCode
        for code in pendingCode:
            exec(code, globalSyms, {})
        pendingCode = []

    @ffi.def_extern()
    def mPythonDebuggerEntered(reason, info):
        debugger = globalSyms['debugger']
        if not debugger:
            return
        if info == ffi.NULL:
            info = None
        for cb in debugger._cbs:
            cb(reason, info)

    @ffi.def_extern()
    def mPythonLookupSymbol(name, outptr):
        name = ffi.string(name).decode('utf-8')
        if name not in symbols:
            return False
        sym = symbols[name]
        val = None
        try:
            val = int(sym)
        except:
            try:
                val = sym()
            except:
                pass
        if val is None:
            return False
        try:
            outptr[0] = ffi.cast('int32_t', val)
            return True
        except:
            return False
""")

if __name__ == "__main__":
    ffi.emit_c_code("lib.c")
