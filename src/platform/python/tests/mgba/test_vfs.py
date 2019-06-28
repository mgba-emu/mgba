import pytest
import os

import mgba.vfs as vfs
from mgba._pylib import ffi


def test_vfs_open():
    with open(__file__) as f:
        vf = vfs.open(f)
        assert vf
        assert vf.close()


def test_vfs_open_path():
    vf = vfs.open_path(__file__)
    assert vf
    assert vf.close()


def test_vfs_read():
    vf = vfs.open_path(__file__)
    buffer = ffi.new('char[13]')
    assert vf.read(buffer, 13) == 13
    assert ffi.string(buffer) == b'import pytest'
    vf.close()


def test_vfs_readline():
    vf = vfs.open_path(__file__)
    buffer = ffi.new('char[16]')
    linelen = vf.readline(buffer, 16)
    assert linelen in (14, 15)
    if linelen == 14:
        assert ffi.string(buffer) == b'import pytest\n'
    elif linelen == 15:
        assert ffi.string(buffer) == b'import pytest\r\n'
    vf.close()


def test_vfs_read_all_size():
    vf = vfs.open_path(__file__)
    buffer = vf.read_all()
    assert buffer
    assert len(buffer)
    assert len(buffer) == vf.size()
    vf.close()


def test_vfs_seek():
    vf = vfs.open_path(__file__)
    assert vf.seek(0, os.SEEK_SET) == 0
    assert vf.seek(1, os.SEEK_SET) == 1
    assert vf.seek(1, os.SEEK_CUR) == 2
    assert vf.seek(-1, os.SEEK_CUR) == 1
    assert vf.seek(0, os.SEEK_CUR) == 1
    assert vf.seek(0, os.SEEK_END) == vf.size()
    assert vf.seek(-1, os.SEEK_END) == vf.size() -1
    vf.close()


def test_vfs_open_path_invalid():
    vf = vfs.open_path('.invalid')
    assert not vf
