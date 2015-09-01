/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "overrides.h"

#include "gba/gba.h"
#include "gba/hardware.h"

#include "util/configuration.h"

static const struct GBACartridgeOverride _overrides[] = {
	// Advance Wars
	{ "AWRE", SAVEDATA_FLASH512, HW_NONE, 0x8038810 },
	{ "AWRP", SAVEDATA_FLASH512, HW_NONE, 0x8038810 },

	// Advance Wars 2: Black Hole Rising
	{ "AW2E", SAVEDATA_FLASH512, HW_NONE, 0x8036E08 },
	{ "AW2P", SAVEDATA_FLASH512, HW_NONE, 0x803719C },

	// Boktai: The Sun is in Your Hand
	{ "U3IJ", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE },
	{ "U3IE", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE },
	{ "U3IP", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE },

	// Boktai 2: Solar Boy Django
	{ "U32J", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE },
	{ "U32E", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE },
	{ "U32P", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE },

	// Dragon Ball Z - The Legacy of Goku
	{ "ALGP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE },

	// Dragon Ball Z - Taiketsu
	{ "BDBE", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE },
	{ "BDBP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE },

	// Drill Dozer
	{ "V49J", SAVEDATA_SRAM, HW_RUMBLE, IDLE_LOOP_NONE },
	{ "V49E", SAVEDATA_SRAM, HW_RUMBLE, IDLE_LOOP_NONE },

	// Final Fantasy Tactics Advance
	{ "AFXE", SAVEDATA_FLASH512, HW_NONE, 0x8000428 },

	// F-Zero - Climax
	{ "BFTJ", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },

	// Golden Sun: The Lost Age
	{ "AGFE", SAVEDATA_FLASH512, HW_NONE, 0x801353A },

	// Koro Koro Puzzle - Happy Panechu!
	{ "KHPJ", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE },

	// Mega Man Battle Network
	{ "AREE", SAVEDATA_SRAM, HW_NONE, 0x800032E },

	// Mega Man Zero
	{ "AZCE", SAVEDATA_SRAM, HW_NONE, 0x80004E8 },

	// Metal Slug Advance
	{ "BSME", SAVEDATA_EEPROM, HW_NONE, 0x8000290 },

	// Pokemon Ruby
	{ "AXVJ", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXVE", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXVP", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXVI", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXVS", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXVD", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXVF", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },

	// Pokemon Sapphire
	{ "AXPJ", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXPE", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXPP", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXPI", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXPS", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXPD", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },
	{ "AXPF", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },

	// Pokemon Emerald
	{ "BPEJ", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6 },
	{ "BPEE", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6 },
	{ "BPEP", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6 },
	{ "BPEI", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6 },
	{ "BPES", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6 },
	{ "BPED", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6 },
	{ "BPEF", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6 },

	// Pokemon Mystery Dungeon
	{ "B24J", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "B24E", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "B24P", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "B24U", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },

	// Pokemon FireRed
	{ "BPRJ", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPRE", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPRP", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPRI", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPRS", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPRD", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPRF", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },

	// Pokemon LeafGreen
	{ "BPGJ", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPGE", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPGP", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPGI", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPGS", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPGD", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },
	{ "BPGF", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE },

	// RockMan EXE 4.5 - Real Operation
	{ "BR4J", SAVEDATA_FLASH512, HW_RTC, IDLE_LOOP_NONE },

	// Rocky
	{ "AR8E", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE },
	{ "AROP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE },

	// Sennen Kazoku
	{ "BKAJ", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE },

	// Shin Bokura no Taiyou: Gyakushuu no Sabata
	{ "U33J", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE },

	// Super Mario Advance 2
	{ "AA2J", SAVEDATA_EEPROM, HW_NONE, 0x800052E },
	{ "AA2E", SAVEDATA_EEPROM, HW_NONE, 0x800052E },
	{ "AA2P", SAVEDATA_EEPROM, HW_NONE, 0x800052E },

	// Super Mario Advance 3
	{ "A3AJ", SAVEDATA_EEPROM, HW_NONE, 0x8002B9C },
	{ "A3AE", SAVEDATA_EEPROM, HW_NONE, 0x8002B9C },
	{ "A3AP", SAVEDATA_EEPROM, HW_NONE, 0x8002B9C },

	// Super Mario Advance 4
	{ "AX4J", SAVEDATA_FLASH1M, HW_NONE, 0x800072A },
	{ "AX4E", SAVEDATA_FLASH1M, HW_NONE, 0x800072A },
	{ "AX4P", SAVEDATA_FLASH1M, HW_NONE, 0x800072A },

	// Top Gun - Combat Zones
	{ "A2YE", SAVEDATA_FORCE_NONE, HW_NONE, IDLE_LOOP_NONE },

	// Wario Ware Twisted
	{ "RZWJ", SAVEDATA_SRAM, HW_RUMBLE | HW_GYRO, IDLE_LOOP_NONE },
	{ "RZWE", SAVEDATA_SRAM, HW_RUMBLE | HW_GYRO, IDLE_LOOP_NONE },
	{ "RZWP", SAVEDATA_SRAM, HW_RUMBLE | HW_GYRO, IDLE_LOOP_NONE },

	// Yoshi's Universal Gravitation
	{ "KYGJ", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE },
	{ "KYGE", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE },
	{ "KYGP", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE },

	{ { 0, 0, 0, 0 }, 0, 0, IDLE_LOOP_NONE }
};

bool GBAOverrideFind(const struct Configuration* config, struct GBACartridgeOverride* override) {
	override->savetype = SAVEDATA_AUTODETECT;
	override->hardware = HW_NONE;
	override->idleLoop = IDLE_LOOP_NONE;
	bool found = false;

	if (override->id[0] == 'F') {
		// Classic NES Series
		override->savetype = SAVEDATA_EEPROM;
		found = true;
	} else {
		int i;
		for (i = 0; _overrides[i].id[0]; ++i) {
			if (memcmp(override->id, _overrides[i].id, sizeof(override->id)) == 0) {
				*override = _overrides[i];
				found = true;
				break;
			}
		}
	}

	if (config) {
		char sectionName[16];
		snprintf(sectionName, sizeof(sectionName), "override.%c%c%c%c", override->id[0], override->id[1], override->id[2], override->id[3]);
		const char* savetype = ConfigurationGetValue(config, sectionName, "savetype");
		const char* hardware = ConfigurationGetValue(config, sectionName, "hardware");
		const char* idleLoop = ConfigurationGetValue(config, sectionName, "idleLoop");

		if (savetype) {
			if (strcasecmp(savetype, "SRAM") == 0) {
				found = true;
				override->savetype = SAVEDATA_SRAM;
			} else if (strcasecmp(savetype, "EEPROM") == 0) {
				found = true;
				override->savetype = SAVEDATA_EEPROM;
			} else if (strcasecmp(savetype, "FLASH512") == 0) {
				found = true;
				override->savetype = SAVEDATA_FLASH512;
			} else if (strcasecmp(savetype, "FLASH1M") == 0) {
				found = true;
				override->savetype = SAVEDATA_FLASH1M;
			} else if (strcasecmp(savetype, "NONE") == 0) {
				found = true;
				override->savetype = SAVEDATA_FORCE_NONE;
			}
		}

		if (hardware) {
			char* end;
			long type = strtoul(hardware, &end, 0);
			if (end && !*end) {
				override->hardware = type;
				found = true;
			}
		}

		if (idleLoop) {
			char* end;
			uint32_t address = strtoul(idleLoop, &end, 16);
			if (end && !*end) {
				override->idleLoop = address;
				found = true;
			}
		}
	}
	return found;
}

void GBAOverrideSave(struct Configuration* config, const struct GBACartridgeOverride* override) {
	char sectionName[16];
	snprintf(sectionName, sizeof(sectionName), "override.%c%c%c%c", override->id[0], override->id[1], override->id[2], override->id[3]);
	const char* savetype = 0;
	switch (override->savetype) {
	case SAVEDATA_SRAM:
		savetype = "SRAM";
		break;
	case SAVEDATA_EEPROM:
		savetype = "EEPROM";
		break;
	case SAVEDATA_FLASH512:
		savetype = "FLASH512";
		break;
	case SAVEDATA_FLASH1M:
		savetype = "FLASH1M";
		break;
	case SAVEDATA_FORCE_NONE:
		savetype = "NONE";
		break;
	case SAVEDATA_AUTODETECT:
		break;
	}
	ConfigurationSetValue(config, sectionName, "savetype", savetype);

	if (override->hardware != HW_NO_OVERRIDE) {
		ConfigurationSetIntValue(config, sectionName, "hardware", override->hardware);
	} else {
		ConfigurationClearValue(config, sectionName, "hardware");
	}

	if (override->idleLoop != IDLE_LOOP_NONE) {
		ConfigurationSetUIntValue(config, sectionName, "idleLoop", override->idleLoop);
	} else {
		ConfigurationClearValue(config, sectionName, "idleLoop");
	}
}

void GBAOverrideApply(struct GBA* gba, const struct GBACartridgeOverride* override) {
	if (override->savetype != SAVEDATA_AUTODETECT) {
		GBASavedataForceType(&gba->memory.savedata, override->savetype, gba->realisticTiming);
	}

	if (override->hardware != HW_NO_OVERRIDE) {
		GBAHardwareClear(&gba->memory.hw);

		if (override->hardware & HW_RTC) {
			GBAHardwareInitRTC(&gba->memory.hw);
		}

		if (override->hardware & HW_GYRO) {
			GBAHardwareInitGyro(&gba->memory.hw);
		}

		if (override->hardware & HW_RUMBLE) {
			GBAHardwareInitRumble(&gba->memory.hw);
		}

		if (override->hardware & HW_LIGHT_SENSOR) {
			GBAHardwareInitLight(&gba->memory.hw);
		}

		if (override->hardware & HW_TILT) {
			GBAHardwareInitTilt(&gba->memory.hw);
		}

		if (override->hardware & HW_GB_PLAYER_DETECTION) {
			gba->memory.hw.devices |= HW_GB_PLAYER_DETECTION;
		} else {
			gba->memory.hw.devices &= ~HW_GB_PLAYER_DETECTION;
		}
	}

	if (override->idleLoop != IDLE_LOOP_NONE) {
		gba->idleLoop = override->idleLoop;
		if (gba->idleOptimization == IDLE_LOOP_DETECT) {
			gba->idleOptimization = IDLE_LOOP_REMOVE;
		}
	}
}

void GBAOverrideApplyDefaults(struct GBA* gba) {
	struct GBACartridgeOverride override;
	const struct GBACartridge* cart = (const struct GBACartridge*) gba->memory.rom;
	memcpy(override.id, &cart->id, sizeof(override.id));
	if (GBAOverrideFind(0, &override)) {
		GBAOverrideApply(gba, &override);
	}
}
