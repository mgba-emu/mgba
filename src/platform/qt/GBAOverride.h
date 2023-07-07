/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Override.h"

#include <mgba/internal/gba/overrides.h>

namespace QGBA {

class GBAOverride : public Override {
public:
	void identify(const struct mCore*) override;
	void save(struct Configuration*) const override;
	const void* raw() const override;

	struct GBACartridgeOverride override;
	bool vbaBugCompatSet;
};

}
