/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_EXTENSIONS_IDS_H
#define GBA_EXTENSIONS_IDS_H

enum GBA_EXTENSIONS_IDS {
    HWEX_ID_MORE_RAM = 0,
    HWEX_EXTENSIONS_COUNT
};

#define HWEX_FLAGS_REGISTERS_COUNT ((HWEX_EXTENSIONS_COUNT / 16) + (HWEX_EXTENSIONS_COUNT % 16 ? 1 : 0))

#endif
