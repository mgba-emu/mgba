# Copyright (c) 2013-2017 Jeffrey Pfau
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
try:
    import mgba_gamedata
except ImportError:
    pass


def search(core):
    crc32 = None
    if hasattr(core, 'PLATFORM_GBA') and core.platform == core.PLATFORM_GBA:
        platform = 'GBA'
        crc32 = core.crc32
    if hasattr(core, 'PLATFORM_GB') and core.platform == core.PLATFORM_GB:
        platform = 'GB'
        crc32 = core.crc32
    cls = mgba_gamedata.registry.search(platform, {'crc32': crc32})
    if not cls:
        return None
    return cls(core.memory.u8)
