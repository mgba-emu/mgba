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
	// None yet
	{ 0, 0, 0, { 0 } }
};

bool GBOverrideFind(const struct Configuration* config, struct GBCartridgeOverride* override) {
	override->model = GB_MODEL_AUTODETECT;
	override->mbc = GB_MBC_AUTODETECT;
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
		const char* pal[4] = {
			ConfigurationGetValue(config, sectionName, "pal[0]"),
			ConfigurationGetValue(config, sectionName, "pal[1]"),
			ConfigurationGetValue(config, sectionName, "pal[2]"),
			ConfigurationGetValue(config, sectionName, "pal[3]")
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

		if (pal[0] && pal[1] && pal[2] && pal[3]) {
			int i;
			for (i = 0; i < 4; ++i) {
				char* end;
				unsigned long value = strtoul(pal[i], &end, 10);
				if (end == &pal[i][1] && *end == 'x') {
					value = strtoul(pal[i], &end, 16);
				}
				if (*end) {
					continue;
				}
				override->gbColors[i] = value;
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

	if (override->gbColors[0] | override->gbColors[1] | override->gbColors[2] | override->gbColors[3]) {
		ConfigurationSetIntValue(config, sectionName, "pal[0]", override->gbColors[0]);
		ConfigurationSetIntValue(config, sectionName, "pal[1]", override->gbColors[1]);
		ConfigurationSetIntValue(config, sectionName, "pal[2]", override->gbColors[2]);
		ConfigurationSetIntValue(config, sectionName, "pal[3]", override->gbColors[3]);
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

	if (override->gbColors[0] | override->gbColors[1] | override->gbColors[2] | override->gbColors[3]) {
		GBVideoSetPalette(&gb->video, 0, override->gbColors[0]);
		GBVideoSetPalette(&gb->video, 1, override->gbColors[1]);
		GBVideoSetPalette(&gb->video, 2, override->gbColors[2]);
		GBVideoSetPalette(&gb->video, 3, override->gbColors[3]);
	}
}

void GBOverrideApplyDefaults(struct GB* gb) {
	struct GBCartridgeOverride override;
	override.headerCrc32 = doCrc32(&gb->memory.rom[0x100], sizeof(struct GBCartridge));
	if (GBOverrideFind(0, &override)) {
		GBOverrideApply(gb, &override);
	}
}
