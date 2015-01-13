/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-overrides.h"

#include "gba.h"
#include "gba-gpio.h"

 #include "util/configuration.h"

static const struct GBACartridgeOverride _overrides[] = {
	// Boktai: The Sun is in Your Hand
	{ "U3IJ", SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR, -1 },
	{ "U3IE", SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR, -1 },
	{ "U3IP", SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR, -1 },

	// Boktai 2: Solar Boy Django
	{ "U32J", SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR, -1 },
	{ "U32E", SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR, -1 },
	{ "U32P", SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR, -1 },

	// Drill Dozer
	{ "V49J", SAVEDATA_SRAM, GPIO_RUMBLE, -1 },
	{ "V49E", SAVEDATA_SRAM, GPIO_RUMBLE, -1 },

	// Final Fantasy Tactics Advance
	{ "AFXE", SAVEDATA_FLASH512, GPIO_NONE, 0x8000418 },

	// Koro Koro Puzzle - Happy Panechu!
	{ "KHPJ", SAVEDATA_EEPROM, GPIO_TILT, -1 },

	// Mega Man Battle Network
	{ "AREE", SAVEDATA_SRAM, GPIO_NONE, 0x800032E },

	// Pokemon Ruby
	{ "AXVJ", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVE", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVP", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVI", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVS", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVD", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVF", SAVEDATA_FLASH1M, GPIO_RTC, -1 },

	// Pokemon Sapphire
	{ "AXPJ", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPE", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPP", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPI", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPS", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPD", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPF", SAVEDATA_FLASH1M, GPIO_RTC, -1 },

	// Pokemon Emerald
	{ "BPEJ", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEE", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEP", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEI", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPES", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPED", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEF", SAVEDATA_FLASH1M, GPIO_RTC, -1 },

	// Pokemon Mystery Dungeon
	{ "B24J", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "B24E", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "B24P", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "B24U", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// Pokemon FireRed
	{ "BPRJ", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPRE", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPRP", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// Pokemon LeafGreen
	{ "BPGJ", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPGE", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPGP", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// RockMan EXE 4.5 - Real Operation
	{ "BR4J", SAVEDATA_FLASH512, GPIO_RTC, -1 },

	// Shin Bokura no Taiyou: Gyakushuu no Sabata
	{ "U33J", SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR, -1 },

	// Super Mario Advance 4
	{ "AX4J", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "AX4E", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "AX4P", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// Top Gun - Combat Zones
	{ "A2YE", SAVEDATA_FORCE_NONE, GPIO_NONE, -1 },

	// Wario Ware Twisted
	{ "RZWJ", SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO, -1 },
	{ "RZWE", SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO, -1 },
	{ "RZWP", SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO, -1 },

	// Yoshi's Universal Gravitation
	{ "KYGJ", SAVEDATA_EEPROM, GPIO_TILT, -1 },
	{ "KYGE", SAVEDATA_EEPROM, GPIO_TILT, -1 },
	{ "KYGP", SAVEDATA_EEPROM, GPIO_TILT, -1 },

	{ { 0, 0, 0, 0 }, 0, 0, -1 }
};

bool GBAOverrideFind(const struct Configuration* config, struct GBACartridgeOverride* override) {
	override->savetype = SAVEDATA_AUTODETECT;
	override->hardware = GPIO_NONE;
	override->idleLoop = -1;
	bool found;

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

void GBAOverrideApply(struct GBA* gba, const struct GBACartridgeOverride* override) {
	GBASavedataForceType(&gba->memory.savedata, override->savetype);

	if (override->hardware & GPIO_RTC) {
		GBAGPIOInitRTC(&gba->memory.gpio);
	}

	if (override->hardware & GPIO_GYRO) {
		GBAGPIOInitGyro(&gba->memory.gpio);
	}

	if (override->hardware & GPIO_RUMBLE) {
		GBAGPIOInitRumble(&gba->memory.gpio);
	}

	if (override->hardware & GPIO_LIGHT_SENSOR) {
		GBAGPIOInitLightSensor(&gba->memory.gpio);
	}

	if (override->hardware & GPIO_TILT) {
		GBAGPIOInitTilt(&gba->memory.gpio);
	}

	gba->busyLoop = override->idleLoop;
}
