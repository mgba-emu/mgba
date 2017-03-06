/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/wifi.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/ds/ds.h>

mLOG_DEFINE_CATEGORY(DS_WIFI, "DS Wi-Fi", "ds.wifi");

void DSWifiReset(struct DS* ds) {
	memset(ds->wifi.io, 0, sizeof(ds->wifi.io));
	memset(ds->wifi.wram, 0, sizeof(ds->wifi.wram));
}

static void DSWifiWriteBB(struct DS* ds, uint8_t address, uint8_t value) {
	mLOG(DS_WIFI, STUB, "Stub Wi-Fi baseband register write: %02X:%02X", address, value);
	ds->wifi.baseband[address] = value;
}

static uint8_t DSWifiReadBB(struct DS* ds, uint8_t address) {
	mLOG(DS_WIFI, STUB, "Stub Wi-Fi baseband register read: %02X", address);
	return ds->wifi.baseband[address];
}

static void DSWifiWriteReg(struct DS* ds, uint32_t address, uint16_t value) {
	switch (address) {
	case 0x158:
		if (value & 0x1000) {
			DSWifiWriteBB(ds, value & 0xFF, ds->wifi.io[0x15A >> 1]);
		}
		if (value & 0x2000) {
			ds->wifi.io[0x15C >> 1] = DSWifiReadBB(ds, value & 0xFF);
		}
		break;
	case 0x15A:
		break;
	default:
		mLOG(DS_WIFI, STUB, "Stub Wi-Fi I/O register write: %06X:%04X", address, value);
		break;
	}
	ds->wifi.io[address >> 1] = value;
}

static uint16_t DSWifiReadReg(struct DS* ds, uint32_t address) {
	switch (address) {
	case 0x15C:
		break;
	default:
		mLOG(DS_WIFI, STUB, "Stub Wi-Fi I/O register read: %06X", address);
		break;
	}
	return ds->wifi.io[address >> 1];
}

void DSWifiWriteIO(struct DS* ds, uint32_t address, uint16_t value) {
	switch (address >> 12) {
	case 0:
	case 1:
	case 6:
	case 7:
		DSWifiWriteReg(ds, address & 0xFFF, value);
		break;
	case 4:
	case 5:
		STORE_16(value, address & 0x1FFE, ds->wifi.wram);
		break;
	}
}

uint16_t DSWifiReadIO(struct DS* ds, uint32_t address) {
	uint16_t value = 0;
	switch (address >> 12) {
	case 0:
	case 1:
	case 6:
	case 7:
		value = DSWifiReadReg(ds, address & 0xFFF);
		break;
	case 4:
	case 5:
		LOAD_16(value, address & 0x1FFE, ds->wifi.wram);
		break;
	}
	return value;
}
