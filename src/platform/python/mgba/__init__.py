# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib

from collections import namedtuple

def createCallback(structName, cbName, funcName=None):
    funcName = funcName or "_py{}{}".format(structName, cbName[0].upper() + cbName[1:])
    fullStruct = "struct {}*".format(structName)
    def cb(handle, *args):
        h = ffi.cast(fullStruct, handle)
        return getattr(ffi.from_handle(h.pyobj), cbName)(*args)

    return ffi.def_extern(name=funcName)(cb)

version = ffi.string(lib.projectVersion).decode('utf-8')

GitInfo = namedtuple("GitInfo", "commit commitShort branch revision")

git = {}
if lib.gitCommit and lib.gitCommit != "(unknown)":
    git['commit'] = ffi.string(lib.gitCommit).decode('utf-8')
if lib.gitCommitShort and lib.gitCommitShort != "(unknown)":
    git['commitShort'] = ffi.string(lib.gitCommitShort).decode('utf-8')
if lib.gitBranch and lib.gitBranch != "(unknown)":
    git['branch'] = ffi.string(lib.gitBranch).decode('utf-8')
if lib.gitRevision > 0:
    git['revision'] = lib.gitRevision

git = GitInfo(**git)
