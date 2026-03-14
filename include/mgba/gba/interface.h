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
	GBA_SIO_NORMAL_8 = 0,
	GBA_SIO_NORMAL_32 = 1,
	GBA_SIO_MULTI = 2,
	GBA_SIO_UART = 3,
	GBA_SIO_GPIO = 8,
	GBA_SIO_JOYBUS = 12
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
	HW_EREADER = 128,

	HW_GPIO = HW_RTC | HW_RUMBLE | HW_LIGHT_SENSOR | HW_GYRO | HW_TILT,
};

struct Configuration;
struct GBAAudio;
struct GBASIO;
struct GBAVideoRenderer;
struct VFile;

extern MGBA_EXPORT const int GBA_LUX_LEVELS[10];

enum {
	mPERIPH_GBA_LUMINANCE = 0x1000,
	mPERIPH_GBA_LINK_PORT,
};

struct GBACartridgeOverride {
	char id[4] ATTRIBUTE_NONSTRING;
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
bool GBAOverrideFindConfig(const struct Configuration*, struct GBACartridgeOverride* override);
void GBAOverrideSave(struct Configuration*, const struct GBACartridgeOverride* override);

struct GBASIODriver {
	struct GBASIO* p;

	bool (*init)(struct GBASIODriver* driver);
	void (*deinit)(struct GBASIODriver* driver);
	void (*reset)(struct GBASIODriver* driver);
	uint32_t (*driverId)(const struct GBASIODriver* renderer);
	bool (*loadState)(struct GBASIODriver* renderer, const void* state, size_t size);
	void (*saveState)(struct GBASIODriver* renderer, void** state, size_t* size);
	void (*setMode)(struct GBASIODriver* driver, enum GBASIOMode mode);
	bool (*handlesMode)(struct GBASIODriver* driver, enum GBASIOMode mode);
	int (*connectedDevices)(struct GBASIODriver* driver);
	int (*deviceId)(struct GBASIODriver* driver);
	uint16_t (*writeSIOCNT)(struct GBASIODriver* driver, uint16_t value);
	uint16_t (*writeRCNT)(struct GBASIODriver* driver, uint16_t value);
	bool (*start)(struct GBASIODriver* driver);
	void (*finishMultiplayer)(struct GBASIODriver* driver, uint16_t data[4]);
	uint8_t (*finishNormal8)(struct GBASIODriver* driver);
	uint32_t (*finishNormal32)(struct GBASIODriver* driver);
};

enum GBASIOBattleChipGateFlavor {
	GBA_FLAVOR_BATTLECHIP_GATE = 4,
	GBA_FLAVOR_PROGRESS_GATE = 5,
	GBA_FLAVOR_BEAST_LINK_GATE = 6,
	GBA_FLAVOR_BEAST_LINK_GATE_US = 7,
};

struct GBASIOBattlechipGate {
	struct GBASIODriver d;
	uint16_t chipId;
	uint16_t data[2];
	int state;
	int flavor;
};

void GBASIOBattlechipGateCreate(struct GBASIOBattlechipGate*);

struct GBA;
void GBACartEReaderQueueCard(struct GBA* gba, const void* data, size_t size);

struct EReaderScan;
#if defined(USE_PNG) && defined(ENABLE_VFS)
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImagePNG(const char* filename);
#endif
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImage(const void* pixels, unsigned width, unsigned height, unsigned stride);
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImageA(const void* pixels, unsigned width, unsigned height, unsigned stride);
MGBA_EXPORT struct EReaderScan* EReaderScanLoadImage8(const void* pixels, unsigned width, unsigned height, unsigned stride);
MGBA_EXPORT void EReaderScanDestroy(struct EReaderScan*);

MGBA_EXPORT bool EReaderScanCard(struct EReaderScan*);
MGBA_EXPORT void EReaderScanOutputBitmap(const struct EReaderScan*, void* output, size_t stride);
#ifdef ENABLE_VFS
MGBA_EXPORT bool EReaderScanSaveRaw(const struct EReaderScan*, const char* filename, bool strict);
#endif

CXX_GUARD_END

#endif
