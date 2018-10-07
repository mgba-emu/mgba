# Copyright (c) 2013-2018 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from ._pylib import ffi, lib  # pylint: disable=no-name-in-module


class Buffer(object):
    def __init__(self, native, internal_rate):
        self._native = native
        self._internal_rate = internal_rate

    @property
    def available(self):
        return lib.blip_samples_avail(self._native)

    def set_rate(self, rate):
        lib.blip_set_rates(self._native, self._internal_rate, rate)

    def read(self, samples):
        buffer = ffi.new("short[%i]" % samples)
        count = self.read_into(buffer, samples, 1, 0)
        return buffer[:count]

    def read_into(self, buffer, samples, channels=1, interleave=0):
        return lib.blip_read_samples(self._native, ffi.addressof(buffer, interleave), samples, channels == 2)

    def clear(self):
        lib.blip_clear(self._native)


class StereoBuffer(object):
    def __init__(self, left, right):
        self._left = left
        self._right = right

    @property
    def available(self):
        return min(self._left.available, self._right.available)

    def set_rate(self, rate):
        self._left.set_rate(rate)
        self._right.set_rate(rate)

    def read(self, samples):
        buffer = ffi.new("short[%i]" % (2 * samples))
        count = self.read_into(buffer, samples)
        return buffer[0:2 * count]

    def read_into(self, buffer, samples):
        samples = self._left.read_into(buffer, samples, 2, 0)
        return self._right.read_into(buffer, samples, 2, 1)

    def clear(self):
        self._left.clear()
        self._right.clear()
