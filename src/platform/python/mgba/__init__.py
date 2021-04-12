# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module


class Git:
    commit = None
    if lib.gitCommit and lib.gitCommit != "(unknown)":
        commit = ffi.string(lib.gitCommit).decode('utf-8')

    commitShort = None
    if lib.gitCommitShort and lib.gitCommitShort != "(unknown)":
        commitShort = ffi.string(lib.gitCommitShort).decode('utf-8')

    branch = None
    if lib.gitBranch and lib.gitBranch != "(unknown)":
        branch = ffi.string(lib.gitBranch).decode('utf-8')

    revision = None
    if lib.gitRevision > 0:
        revision = lib.gitRevision


def create_callback(struct_name, cb_name, func_name=None):
    func_name = func_name or "_py{}{}".format(struct_name, cb_name[0].upper() + cb_name[1:])
    full_struct = "struct {}*".format(struct_name)

    def callback(handle, *args):
        handle = ffi.cast(full_struct, handle)
        return getattr(ffi.from_handle(handle.pyobj), cb_name)(*args)

    return ffi.def_extern(name=func_name)(callback)


__version__ = ffi.string(lib.projectVersion).decode('utf-8')
