/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_MBC_H
#define GB_MBC_H

#include "util/common.h"

#include "core/log.h"

mLOG_DECLARE_CATEGORY(GB_MBC);

struct GB;
struct GBMemory;
void GBMBCInit(struct GB* gb);
void GBMBCSwitchBank(struct GBMemory* memory, int bank);
void GBMBCSwitchSramBank(struct GB* gb, int bank);

uint8_t GBMBC7Read(struct GBMemory*, uint16_t address);
void GBMBC7Write(struct GBMemory*, uint16_t address, uint8_t value);

#endif
