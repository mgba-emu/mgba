/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_INTERFACE_H
#define GBA_INTERFACE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>
#include <mgba/core/timing.h>

#define GBA_IDLE_LOOP_NONE 0xFFFFFFFF

enum {
	GBA_VIDEO_HORIZONTAL_PIXELS = 240,
	GBA_VIDEO_VERTICAL_PIXELS = 160,
};

enum GBASIOMode {
	SIO_NORMAL_8 = 0,
	SIO_NORMAL_32 = 1,
	SIO_MULTI = 2,
	SIO_UART = 3,
	SIO_GPIO = 8,
	SIO_JOYBUS = 12
};

enum GBASIOJOYCommand {
	JOY_RESET = 0xFF,
	JOY_POLL = 0x00,
	JOY_TRANS = 0x14,
	JOY_RECV = 0x15
};

enum GBAVideoLayer {
	GBA_LAYER_BG0 = 0,
	GBA_LAYER_BG1,
	GBA_LAYER_BG2,
	GBA_LAYER_BG3,
	GBA_LAYER_OBJ,
	GBA_LAYER_WIN0,
	GBA_LAYER_WIN1,
	GBA_LAYER_OBJWIN,
};

enum GBASavedataType {
	GBA_SAVEDATA_AUTODETECT = -1,
	GBA_SAVEDATA_FORCE_NONE = 0,
	GBA_SAVEDATA_SRAM = 1,
	GBA_SAVEDATA_FLASH512 = 2,
	GBA_SAVEDATA_FLASH1M = 3,
	GBA_SAVEDATA_EEPROM = 4,
	GBA_SAVEDATA_EEPROM512 = 5,
	GBA_SAVEDATA_SRAM512 = 6,
};

enum GBAHardwareDevice {
	HW_NO_OVERRIDE = 0x8000,
	HW_NONE = 0,
	HW_RTC = 1,
	HW_RUMBLE = 2,
	HW_LIGHT_SENSOR = 4,
	HW_GYRO = 8,
	HW_TILT = 16,
	HW_GB_PLAYER = 32,
	HW_GB_PLAYER_DETECTION = 64,
	HW_EREADER = 128
};

struct Configuration;
struct GBAAudio;
struct GBASIO;
struct GBAVideoRenderer;
struct VFile;

extern MGBA_EXPORT const int GBA_LUX_LEVELS[10];

enum {
	mPERIPH_GBA_LUMINANCE = 0x1000,
	mPERIPH_GBA_BATTLECHIP_GATE,
};

struct GBACartridgeOverride {
	char id[4];
	enum GBASavedataType savetype;
	int hardware;
	uint32_t idleLoop;
	bool vbaBugCompat;
};

struct GBALuminanceSource {
	void (*sample)(struct GBALuminanceSource*);

	uint8_t (*readLuminance)(struct GBALuminanceSource*);
};

bool GBAIsROM(struct VFile* vf);
bool GBAIsMB(struct VFile* vf);
bool GBAIsBIOS(struct VFile* vf);

bool GBAOverrideFind(const struct Configuration*, struct GBACartridgeOverride* override);
void GBAOverrideSave(struct Configuration*, const struct GBACartridgeOverride* override);

struct GBASIODriver {
	struct GBASIO* p;

	bool (*init)(struct GBASIODriver* driver);
	void (*deinit)(struct GBASIODriver* driver);
	bool (*load)(struct GBASIODriver* driver);
	bool (*unload)(struct GBASIODriver* driver);
	uint16_t (*writeRegister)(struct GBASIODriver* driver, uint32_t address, uint16_t value);
};

void GBASIOJOYCreate(struct GBASIODriver* sio);

enum GBASIOBattleChipGateFlavor {
	GBA_FLAVOR_BATTLECHIP_GATE = 4,
	GBA_FLAVOR_PROGRESS_GATE = 5,
	GBA_FLAVOR_BEAST_LINK_GATE = 6,
	GBA_FLAVOR_BEAST_LINK_GATE_US = 7,
};

struct GBASIOBattlechipGate {
	struct GBASIODriver d;
	struct mTimingEvent event;
	uint16_t chipId;
	uint16_t data[2];
	int state;
	int flavor;
};

void GBASIOBattlechipGateCreate(struct GBASIOBattlechipGate*);

struct GBA;
void GBACartEReaderQueueCard(struct GBA* gba, const void* data, size_t size);

struct EReaderScan;
#ifdef USE_PNG
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImagePNG(const char* filename);
#endif
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImage(const void* pixels, unsigned width, unsigned height, unsigned stride);
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImageA(const void* pixels, unsigned width, unsigned height, unsigned stride);
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImage8(const void* pixels, unsigned width, unsigned height, unsigned stride);
MGBA_EXPORT void EReaderScanDestroy(struct EReaderScan*);

MGBA_EXPORT bool EReaderScanCard(struct EReaderScan*);
MGBA_EXPORT void EReaderScanOutputBitmap(const struct EReaderScan*, void* output, size_t stride);
MGBA_EXPORT bool EReaderScanSaveRaw(const struct EReaderScan*, const char* filename, bool strict);

CXX_GUARD_END

#endif
