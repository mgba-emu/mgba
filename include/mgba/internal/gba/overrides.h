/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_OVERRIDES_H
#define GBA_OVERRIDES_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gba/interface.h>

struct GBA;
void GBAOverrideApply(struct GBA*, const struct GBACartridgeOverride*);
void GBAOverrideApplyDefaults(struct GBA*, const struct Configuration*);

CXX_GUARD_END

#endif
