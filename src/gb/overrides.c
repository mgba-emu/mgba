/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/overrides.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/mbc.h>

#include <mgba-util/configuration.h>
#include <mgba-util/crc32.h>

static const struct GBCartridgeOverride _overrides[] = {
	// Pokemon Spaceworld 1997 demo
	{ 0x232a067d, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Gold (debug)
	{ 0x630ed957, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Gold (non-debug)
	{ 0x5aff0038, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Silver (debug)
	{ 0xa61856bd, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Silver (non-debug)

	{ 0, 0, 0, { 0 } }
};

bool GBOverrideFind(const struct Configuration* config, struct GBCartridgeOverride* override) {
	override->model = GB_MODEL_AUTODETECT;
	override->mbc = GB_MBC_AUTODETECT;
	memset(override->gbColors, 0, sizeof(override->gbColors));
	bool found = false;

	int i;
	for (i = 0; _overrides[i].headerCrc32; ++i) {
		if (override->headerCrc32 == _overrides[i].headerCrc32) {
			*override = _overrides[i];
			found = true;
			break;
		}
	}

	if (config) {
		char sectionName[24] = "";
		snprintf(sectionName, sizeof(sectionName), "gb.override.%08X", override->headerCrc32);
		const char* model = ConfigurationGetValue(config, sectionName, "model");
		const char* mbc = ConfigurationGetValue(config, sectionName, "mbc");
		const char* pal[12] = {
			ConfigurationGetValue(config, sectionName, "pal[0]"),
			ConfigurationGetValue(config, sectionName, "pal[1]"),
			ConfigurationGetValue(config, sectionName, "pal[2]"),
			ConfigurationGetValue(config, sectionName, "pal[3]"),
			ConfigurationGetValue(config, sectionName, "pal[4]"),
			ConfigurationGetValue(config, sectionName, "pal[5]"),
			ConfigurationGetValue(config, sectionName, "pal[6]"),
			ConfigurationGetValue(config, sectionName, "pal[7]"),
			ConfigurationGetValue(config, sectionName, "pal[8]"),
			ConfigurationGetValue(config, sectionName, "pal[9]"),
			ConfigurationGetValue(config, sectionName, "pal[10]"),
			ConfigurationGetValue(config, sectionName, "pal[11]")
		};

		if (model) {
			override->model = GBNameToModel(model);
			found = override->model != GB_MODEL_AUTODETECT;
		}

		if (mbc) {
			char* end;
			long type = strtoul(mbc, &end, 0);
			if (end && !*end) {
				override->mbc = type;
				found = true;
			}
		}

		for (i = 0; i < 12; ++i) {
			if (!pal[i]) {
				continue;
			}
			char* end;
			unsigned long value = strtoul(pal[i], &end, 10);
			if (end == &pal[i][1] && *end == 'x') {
				value = strtoul(pal[i], &end, 16);
			}
			if (*end) {
				continue;
			}
			value |= 0xFF000000;
			override->gbColors[i] = value;
			if (i < 8) {
				override->gbColors[i + 4] = value;
			}
			if (i < 4) {
				override->gbColors[i + 8] = value;
			}
		}
	}
	return found;
}

void GBOverrideSave(struct Configuration* config, const struct GBCartridgeOverride* override) {
	char sectionName[24] = "";
	snprintf(sectionName, sizeof(sectionName), "gb.override.%08X", override->headerCrc32);
	const char* model = GBModelToName(override->model);
	ConfigurationSetValue(config, sectionName, "model", model);

	if (override->gbColors[0] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[0]", override->gbColors[0] & ~0xFF000000);
	}
	if (override->gbColors[1] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[1]", override->gbColors[1] & ~0xFF000000);
	}
	if (override->gbColors[2] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[2]", override->gbColors[2] & ~0xFF000000);
	}
	if (override->gbColors[3] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[3]", override->gbColors[3] & ~0xFF000000);
	}
	if (override->gbColors[4] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[4]", override->gbColors[4] & ~0xFF000000);
	}
	if (override->gbColors[5] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[5]", override->gbColors[5] & ~0xFF000000);
	}
	if (override->gbColors[6] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[6]", override->gbColors[6] & ~0xFF000000);
	}
	if (override->gbColors[7] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[7]", override->gbColors[7] & ~0xFF000000);
	}
	if (override->gbColors[8] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[8]", override->gbColors[8] & ~0xFF000000);
	}
	if (override->gbColors[9] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[9]", override->gbColors[9] & ~0xFF000000);
	}
	if (override->gbColors[10] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[10]", override->gbColors[10] & ~0xFF000000);
	}
	if (override->gbColors[11] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[11]", override->gbColors[11] & ~0xFF000000);
	}

	if (override->mbc != GB_MBC_AUTODETECT) {
		ConfigurationSetIntValue(config, sectionName, "mbc", override->mbc);
	} else {
		ConfigurationClearValue(config, sectionName, "mbc");
	}
}

void GBOverrideApply(struct GB* gb, const struct GBCartridgeOverride* override) {
	if (override->model != GB_MODEL_AUTODETECT) {
		gb->model = override->model;
	}

	if (override->mbc != GB_MBC_AUTODETECT) {
		gb->memory.mbcType = override->mbc;
		GBMBCInit(gb);
	}

	int i;
	for (i = 0; i < 12; ++i) {
		if (!(override->gbColors[i] & 0xFF000000)) {
			continue;
		}
		GBVideoSetPalette(&gb->video, i, override->gbColors[i]);
		if (i < 8) {
			GBVideoSetPalette(&gb->video, i + 4, override->gbColors[i]);
		}
		if (i < 4) {
			GBVideoSetPalette(&gb->video, i + 8, override->gbColors[i]);
		}
	}
}

void GBOverrideApplyDefaults(struct GB* gb) {
	struct GBCartridgeOverride override;
	override.headerCrc32 = doCrc32(&gb->memory.rom[0x100], sizeof(struct GBCartridge));
	if (GBOverrideFind(0, &override)) {
		GBOverrideApply(gb, &override);
	}
}
