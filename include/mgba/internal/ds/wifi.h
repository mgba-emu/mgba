/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_WIFI_H
#define DS_WIFI_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(DS_WIFI);

struct DSWifi {
	uint16_t io[0x800];
	uint16_t wram[0x1000];
	uint8_t baseband[0x100];
};

struct DS;
void DSWifiReset(struct DS* ds);
void DSWifiWriteIO(struct DS* ds, uint32_t address, uint16_t value);
uint16_t DSWifiReadIO(struct DS* ds, uint32_t address);

CXX_GUARD_END

#endif
