import cffi
import distutils.ccompiler
import json
import os, os.path
import re
import shlex
import subprocess
import sys


def split_cmake_list(value):
    return list(filter(None,value.split(";")))


with open(os.environ.get("JSON_CONFIG"), "r") as f:
    config = json.load(f)

default_compiler = distutils.ccompiler.new_compiler()

ffi = cffi.FFI()
pydir = os.path.dirname(os.path.abspath(__file__))
bindir = config["BINDIR"]
libdirs = [config["LIBDIR"]]
incdirs = split_cmake_list(config["INCDIRS"])

cppflags = shlex.split(config["C_FLAGS"])
cppflags += ["-I" + dir for dir in incdirs]
cppflags += ["-D" + definition + "" for definition in split_cmake_list(config["COMPILE_DEFINITIONS"])]

cpplibs = ["mgba"]
for lib in split_cmake_list(config["LINK_LIBRARIES"]):
    dir, name = os.path.split(lib)
    if dir:
        match = re.match(default_compiler.static_lib_format % ('(.+)', default_compiler.static_lib_extension), name)
        if not match:
            match = re.match(default_compiler.shared_lib_format % ('(.+)', default_compiler.shared_lib_extension), name)
        if match:
            name = os.path.join(dir, match[1])
    cpplibs.append(name)
if config["MSVC"]:
    cpplibs += ["ole32", "shell32"]
if config["MINGW"]:
    cpplibs += ["ole32", "uuid"]

features = split_cmake_list(config["FEATURES"])

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
#include <mgba-util/png-io.h>
#include <mgba-util/vfs.h>

#define PYEXPORT
#include "platform/python/core.h"
#include "platform/python/log.h"
#include "platform/python/sio.h"
#include "platform/python/vfs-py.h"
#undef PYEXPORT
""", include_dirs=incdirs,
     extra_compile_args=cppflags,
     libraries=cpplibs,
     library_dirs=libdirs,
     runtime_library_dirs=[bindir] if not config["MSVC"] else None,
     sources=[os.path.join(pydir, path) for path in ["vfs-py.c", "core.c", "log.c", "sio.c"]])


def preprocess_header(header):
    preprocess_flags = ["-E", "-P", "-fno-inline"] if config["C_COMPILER_ID"] != "MSVC" else ["-EP", "-Ob0"]
    preprocessed = subprocess.check_output(
        [config["C_COMPILER"]] + preprocess_flags + cppflags + [os.path.join(pydir, header)], universal_newlines=True)
    res = ""
    for line in preprocessed.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        res += line + "\n"
    return res


ffi.cdef(preprocess_header("_builder.h"))
ffi.embedding_api(preprocess_header("lib.h"))

embedding_init_code = """
    import os, os.path
    from mgba._pylib import ffi, lib
    symbols = {}
    globalSyms = {
        'symbols': symbols
    }
    pendingCode = []

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
"""
if "DEBUGGERS" in features:
    embedding_init_code += """
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
    def mPythonDebuggerEntered(reason, info):
        debugger = globalSyms['debugger']
        if not debugger:
            return
        if info == ffi.NULL:
            info = None
        for cb in debugger._cbs:
            cb(reason, info)
"""
ffi.embedding_init_code(embedding_init_code)

if __name__ == "__main__":
    ffi.emit_c_code("lib.c")
