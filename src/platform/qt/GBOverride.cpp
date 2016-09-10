/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GBOverride.h"

extern "C" {
#include "core/core.h"
}

using namespace QGBA;

void GBOverride::apply(struct mCore* core) {
	if (core->platform(core) != PLATFORM_GB) {
		return;
	}
	GBOverrideApply(static_cast<GB*>(core->board), &override);
}

void GBOverride::save(struct Configuration* config) const {
	GBOverrideSave(config, &override);
}
