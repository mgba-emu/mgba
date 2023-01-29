/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_MBC_PRIVATE_H
#define GB_MBC_PRIVATE_H

#include <mgba/internal/gb/mbc.h>

CXX_GUARD_START

struct GB;
struct GBMemory;
struct mRTCSource;

void _GBMBC1(struct GB*, uint16_t address, uint8_t value);
void _GBMBC2(struct GB*, uint16_t address, uint8_t value);
void _GBMBC3(struct GB*, uint16_t address, uint8_t value);
void _GBMBC5(struct GB*, uint16_t address, uint8_t value);
void _GBMBC6(struct GB*, uint16_t address, uint8_t value);
void _GBMBC7(struct GB*, uint16_t address, uint8_t value);

void _GBMMM01(struct GB*, uint16_t address, uint8_t value);
void _GBPocketCam(struct GB* gb, uint16_t address, uint8_t value);
void _GBTAMA5(struct GB* gb, uint16_t address, uint8_t value);

void _GBHuC1(struct GB*, uint16_t address, uint8_t value);
void _GBHuC3(struct GB*, uint16_t address, uint8_t value);

void _GBWisdomTree(struct GB* gb, uint16_t address, uint8_t value);
void _GBPKJD(struct GB* gb, uint16_t address, uint8_t value);
void _GBNTOld1(struct GB* gb, uint16_t address, uint8_t value);
void _GBNTOld2(struct GB* gb, uint16_t address, uint8_t value);
void _GBNTNew(struct GB* gb, uint16_t address, uint8_t value);
void _GBBBD(struct GB* gb, uint16_t address, uint8_t value);
void _GBHitek(struct GB* gb, uint16_t address, uint8_t value);
void _GBLiCheng(struct GB* gb, uint16_t address, uint8_t value);
void _GBGGB81(struct GB* gb, uint16_t address, uint8_t value);
void _GBSachen(struct GB* gb, uint16_t address, uint8_t value);

uint8_t _GBMBC2Read(struct GBMemory*, uint16_t address);
uint8_t _GBMBC6Read(struct GBMemory*, uint16_t address);
uint8_t _GBMBC7Read(struct GBMemory*, uint16_t address);
void _GBMBC7Write(struct GBMemory* memory, uint16_t address, uint8_t value);

uint8_t _GBPocketCamRead(struct GBMemory*, uint16_t address);
uint8_t _GBTAMA5Read(struct GBMemory*, uint16_t address);
uint8_t _GBHuC3Read(struct GBMemory*, uint16_t address);

uint8_t _GBPKJDRead(struct GBMemory*, uint16_t address);
uint8_t _GBBBDRead(struct GBMemory*, uint16_t address);
uint8_t _GBHitekRead(struct GBMemory*, uint16_t address);
uint8_t _GBGGB81Read(struct GBMemory*, uint16_t address);
uint8_t _GBSachenMMC1Read(struct GBMemory*, uint16_t address);
uint8_t _GBSachenMMC2Read(struct GBMemory*, uint16_t address);

void _GBMBCLatchRTC(struct mRTCSource* rtc, uint8_t* rtcRegs, time_t* rtcLastLatch);
void _GBMBCAppendSaveSuffix(struct GB* gb, const void* buffer, size_t size);

CXX_GUARD_END

#endif
