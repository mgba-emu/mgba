# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib

def createCallback(structName, cbName, funcName=None):
    funcName = funcName or "_py{}{}".format(structName, cbName[0].upper() + cbName[1:])
    fullStruct = "struct {}*".format(structName)
    def cb(handle, *args):
        h = ffi.cast(fullStruct, handle)
        return getattr(ffi.from_handle(h.pyobj), cbName)(*args)

    return ffi.def_extern(name=funcName)(cb)
