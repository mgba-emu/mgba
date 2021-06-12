/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBOverride.h"

#include <mgba/core/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba-util/crc32.h>

using namespace QGBA;

void GBOverride::apply(struct mCore* core) {
	if (core->platform(core) != mPLATFORM_GB) {
		return;
	}
	GBOverrideApply(static_cast<GB*>(core->board), &override);
}

void GBOverride::identify(const struct mCore* core) {
	if (core->platform(core) != mPLATFORM_GB) {
		return;
	}
	GB* gb = static_cast<GB*>(core->board);
	if (!gb->memory.rom || gb->memory.romSize < sizeof(struct GBCartridge) + 0x100) {
		return;
	}
	override.headerCrc32 = doCrc32(&gb->memory.rom[0x100], sizeof(struct GBCartridge));
}

void GBOverride::save(struct Configuration* config) const {
	GBOverrideSave(config, &override);
}
