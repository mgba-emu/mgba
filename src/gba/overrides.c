/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/overrides.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/hardware.h>

#include <mgba-util/configuration.h>

static const struct GBACartridgeOverride _overrides[] = {
	// Advance Wars
	{ "AWRE", SAVEDATA_FLASH512, HW_NONE, 0x8038810, false },
	{ "AWRP", SAVEDATA_FLASH512, HW_NONE, 0x8038810, false },

	// Advance Wars 2: Black Hole Rising
	{ "AW2E", SAVEDATA_FLASH512, HW_NONE, 0x8036E08, false },
	{ "AW2P", SAVEDATA_FLASH512, HW_NONE, 0x803719C, false },

	// Boktai: The Sun is in Your Hand
	{ "U3IJ", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE, false },
	{ "U3IE", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE, false },
	{ "U3IP", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE, false },

	// Boktai 2: Solar Boy Django
	{ "U32J", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE, false },
	{ "U32E", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE, false },
	{ "U32P", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE, false },

	// Crash Bandicoot 2 - N-Tranced
	{ "AC8J", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },
	{ "AC8E", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },
	{ "AC8P", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	// Dragon Ball Z - The Legacy of Goku
	{ "ALGP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	// Dragon Ball Z - The Legacy of Goku II
	{ "ALFJ", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },
	{ "ALFE", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },
	{ "ALFP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	// Dragon Ball Z - Taiketsu
	{ "BDBE", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BDBP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	// Drill Dozer
	{ "V49J", SAVEDATA_SRAM, HW_RUMBLE, IDLE_LOOP_NONE, false },
	{ "V49E", SAVEDATA_SRAM, HW_RUMBLE, IDLE_LOOP_NONE, false },

	// Final Fantasy Tactics Advance
	{ "AFXE", SAVEDATA_FLASH512, HW_NONE, 0x8000428, false },

	// F-Zero - Climax
	{ "BFTJ", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },

	// Iridion II
	{ "AI2E", SAVEDATA_FORCE_NONE, HW_NONE, IDLE_LOOP_NONE, false },
	{ "AI2P", SAVEDATA_FORCE_NONE, HW_NONE, IDLE_LOOP_NONE, false },

	// Golden Sun: The Lost Age
	{ "AGFE", SAVEDATA_FLASH512, HW_NONE, 0x801353A, false },

	// Koro Koro Puzzle - Happy Panechu!
	{ "KHPJ", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE, false },

	// Mega Man Battle Network
	{ "AREE", SAVEDATA_SRAM, HW_NONE, 0x800032E, false },

	// Mega Man Zero
	{ "AZCE", SAVEDATA_SRAM, HW_NONE, 0x80004E8, false },

	// Metal Slug Advance
	{ "BSME", SAVEDATA_EEPROM, HW_NONE, 0x8000290, false },

	// Pokemon Pinball: Ruby & Sapphire
	{ "BPPJ", SAVEDATA_SRAM, HW_GB_PLAYER_DETECTION, IDLE_LOOP_NONE, false },
	{ "BPPE", SAVEDATA_SRAM, HW_GB_PLAYER_DETECTION, IDLE_LOOP_NONE, false },
	{ "BPPP", SAVEDATA_SRAM, HW_GB_PLAYER_DETECTION, IDLE_LOOP_NONE, false },
	{ "BPPU", SAVEDATA_SRAM, HW_GB_PLAYER_DETECTION, IDLE_LOOP_NONE, false },

	// Pokemon Ruby
	{ "AXVJ", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXVE", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXVP", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXVI", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXVS", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXVD", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXVF", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },

	// Pokemon Sapphire
	{ "AXPJ", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXPE", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXPP", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXPI", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXPS", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXPD", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },
	{ "AXPF", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },

	// Pokemon Emerald
	{ "BPEJ", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6, false },
	{ "BPEE", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6, false },
	{ "BPEP", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6, false },
	{ "BPEI", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6, false },
	{ "BPES", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6, false },
	{ "BPED", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6, false },
	{ "BPEF", SAVEDATA_FLASH1M, HW_RTC, 0x80008C6, false },

	// Pokemon Mystery Dungeon
	{ "B24J", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "B24E", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "B24P", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "B24U", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },

	// Pokemon FireRed
	{ "BPRJ", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPRE", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPRP", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPRI", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPRS", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPRD", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPRF", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },

	// Pokemon LeafGreen
	{ "BPGJ", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPGE", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPGP", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPGI", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPGS", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPGD", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },
	{ "BPGF", SAVEDATA_FLASH1M, HW_NONE, IDLE_LOOP_NONE, false },

	// RockMan EXE 4.5 - Real Operation
	{ "BR4J", SAVEDATA_FLASH512, HW_RTC, IDLE_LOOP_NONE, false },

	// Rocky
	{ "AR8E", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },
	{ "AROP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	// Sennen Kazoku
	{ "BKAJ", SAVEDATA_FLASH1M, HW_RTC, IDLE_LOOP_NONE, false },

	// Shin Bokura no Taiyou: Gyakushuu no Sabata
	{ "U33J", SAVEDATA_EEPROM, HW_RTC | HW_LIGHT_SENSOR, IDLE_LOOP_NONE, false },

	// Super Mario Advance 2
	{ "AA2J", SAVEDATA_EEPROM, HW_NONE, 0x800052E, false },
	{ "AA2E", SAVEDATA_EEPROM, HW_NONE, 0x800052E, false },
	{ "AA2P", SAVEDATA_AUTODETECT, HW_NONE, 0x800052E, false },

	// Super Mario Advance 3
	{ "A3AJ", SAVEDATA_EEPROM, HW_NONE, 0x8002B9C, false },
	{ "A3AE", SAVEDATA_EEPROM, HW_NONE, 0x8002B9C, false },
	{ "A3AP", SAVEDATA_EEPROM, HW_NONE, 0x8002B9C, false },

	// Super Mario Advance 4
	{ "AX4J", SAVEDATA_FLASH1M, HW_NONE, 0x800072A, false },
	{ "AX4E", SAVEDATA_FLASH1M, HW_NONE, 0x800072A, false },
	{ "AX4P", SAVEDATA_FLASH1M, HW_NONE, 0x800072A, false },

	// Super Monkey Ball Jr.
	{ "ALUE", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },
	{ "ALUP", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	// Top Gun - Combat Zones
	{ "A2YE", SAVEDATA_FORCE_NONE, HW_NONE, IDLE_LOOP_NONE, false },

	// Ueki no Housoku - Jingi Sakuretsu! Nouryokusha Battle
	{ "BUHJ", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	// Wario Ware Twisted
	{ "RZWJ", SAVEDATA_SRAM, HW_RUMBLE | HW_GYRO, IDLE_LOOP_NONE, false },
	{ "RZWE", SAVEDATA_SRAM, HW_RUMBLE | HW_GYRO, IDLE_LOOP_NONE, false },
	{ "RZWP", SAVEDATA_SRAM, HW_RUMBLE | HW_GYRO, IDLE_LOOP_NONE, false },

	// Yoshi's Universal Gravitation
	{ "KYGJ", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE, false },
	{ "KYGE", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE, false },
	{ "KYGP", SAVEDATA_EEPROM, HW_TILT, IDLE_LOOP_NONE, false },

	// Aging cartridge
	{ "TCHK", SAVEDATA_EEPROM, HW_NONE, IDLE_LOOP_NONE, false },

	{ { 0, 0, 0, 0 }, 0, 0, IDLE_LOOP_NONE, false }
};

bool GBAOverrideFind(const struct Configuration* config, struct GBACartridgeOverride* override) {
	override->savetype = SAVEDATA_AUTODETECT;
	override->hardware = HW_NONE;
	override->idleLoop = IDLE_LOOP_NONE;
	override->mirroring = false;
	bool found = false;

	if (override->id[0] == 'F') {
		// Classic NES Series
		override->savetype = SAVEDATA_EEPROM;
		override->mirroring = true;
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
		GBASavedataForceType(&gba->memory.savedata, override->savetype);
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

	if (override->mirroring) {
		gba->memory.mirroring = true;
	}
}

void GBAOverrideApplyDefaults(struct GBA* gba, const struct Configuration* overrides) {
	struct GBACartridgeOverride override = { .idleLoop = IDLE_LOOP_NONE };
	const struct GBACartridge* cart = (const struct GBACartridge*) gba->memory.rom;
	if (cart) {
		memcpy(override.id, &cart->id, sizeof(override.id));

		if (!strncmp("pokemon red version", &((const char*) gba->memory.rom)[0x108], 20) && gba->romCrc32 != 0xDD88761C) {
			// Enable FLASH1M and RTC on Pokémon FireRed ROM hacks
			override.savetype = SAVEDATA_FLASH1M;
			override.hardware = HW_RTC;
			GBAOverrideApply(gba, &override);
		} else if (GBAOverrideFind(overrides, &override)) {
			GBAOverrideApply(gba, &override);
		}
	}
}
