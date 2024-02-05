/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBAOverride.h"

#include <mgba/core/core.h>
#include <mgba/internal/gba/gba.h>

using namespace QGBA;

void GBAOverride::identify(const struct mCore* core) {
	if (core->platform(core) != mPLATFORM_GBA) {
		return;
	}
	char gameId[8];
	core->getGameCode(core, gameId);
	memcpy(override.id, &gameId[4], 4);
}

void GBAOverride::save(struct Configuration* config) const {
	GBAOverrideSave(config, &override);
}

const void* GBAOverride::raw() const {
	return &override;
}
