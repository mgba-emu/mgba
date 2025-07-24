/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_INTERFACE_H
#define GB_INTERFACE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>

enum GBModel {
	GB_MODEL_AUTODETECT = 0xFF,
	GB_MODEL_DMG  = 0x00,
	GB_MODEL_SGB  = 0x20,
	GB_MODEL_MGB  = 0x40,
	GB_MODEL_SGB2 = GB_MODEL_MGB | GB_MODEL_SGB,
	GB_MODEL_CGB  = 0x80,
	GB_MODEL_SCGB = GB_MODEL_CGB | GB_MODEL_SGB,
	GB_MODEL_AGB  = 0xC0
};

enum GBMemoryBankControllerType {
	GB_MBC_AUTODETECT = -1,
	GB_MBC_NONE = 0,
	GB_MBC1 = 1,
	GB_MBC2 = 2,
	GB_MBC3 = 3,
	GB_MBC5 = 5,
	GB_MBC6 = 6,
	GB_MBC7 = 7,
	GB_MMM01 = 0x10,
	GB_HuC1 = 0x11,
	GB_HuC3 = 0x12,
	GB_POCKETCAM = 0x13,
	GB_TAMA5 = 0x14,
	GB_M161 = 0x15,
	GB_MBC3_RTC = 0x103,
	GB_MBC5_RUMBLE = 0x105,
	GB_UNL_WISDOM_TREE = 0x200,
	GB_UNL_PKJD = 0x203,
	GB_UNL_NT_OLD_1 = 0x210,
	GB_UNL_NT_OLD_2 = 0x211,
	GB_UNL_NT_NEW = 0x212,
	GB_UNL_BBD = 0x220,
	GB_UNL_HITEK = 0x221,
	GB_UNL_LI_CHENG = 0x222,
	GB_UNL_GGB81 = 0x223,
	GB_UNL_SACHEN_MMC1 = 0x230,
	GB_UNL_SACHEN_MMC2 = 0x231,
	GB_UNL_SINTAX = 0x240,
};

enum GBVideoLayer {
	GB_LAYER_BACKGROUND = 0,
	GB_LAYER_WINDOW,
	GB_LAYER_OBJ
};

enum GBColorLookup {
	GB_COLORS_NONE = 0,
	GB_COLORS_CGB = 1,
	GB_COLORS_SGB = 2,
	GB_COLORS_SGB_CGB_FALLBACK = GB_COLORS_CGB | GB_COLORS_SGB
};

struct GBSIODriver {
	struct GBSIO* p;

	bool (*init)(struct GBSIODriver* driver);
	void (*deinit)(struct GBSIODriver* driver);
	void (*writeSB)(struct GBSIODriver* driver, uint8_t value);
	uint8_t (*writeSC)(struct GBSIODriver* driver, uint8_t value);
};

struct GBCartridgeOverride {
	int headerCrc32;
	enum GBModel model;
	enum GBMemoryBankControllerType mbc;

	uint32_t gbColors[12];
};

struct GBColorPreset {
	const char* name;
	uint32_t colors[12];
};

struct Configuration;
struct VFile;

bool GBIsROM(struct VFile* vf);
bool GBIsBIOS(struct VFile* vf);
bool GBIsCompatibleBIOS(struct VFile* vf, enum GBModel model);

enum GBModel GBNameToModel(const char*);
const char* GBModelToName(enum GBModel);

int GBValidModels(const uint8_t* bank0);

bool GBOverrideFind(const struct Configuration*, struct GBCartridgeOverride* override);
bool GBOverrideColorFind(struct GBCartridgeOverride* override, enum GBColorLookup);
void GBOverrideSave(struct Configuration*, const struct GBCartridgeOverride* override);

size_t GBColorPresetList(const struct GBColorPreset** presets);

CXX_GUARD_END

#endif
