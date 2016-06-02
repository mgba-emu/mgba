/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "bios.h"

mLOG_DEFINE_CATEGORY(DS_BIOS, "DS BIOS");

const uint32_t DS7_BIOS_CHECKSUM = 0x1280F0D5;
const uint32_t DS9_BIOS_CHECKSUM = 0x2AB23573;
