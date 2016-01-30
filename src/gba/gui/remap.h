/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_REMAP_H
#define GUI_REMAP_H

#include "util/common.h"

struct GUIInputKeys {
	const char* name;
	uint32_t id;
	const char* const* keyNames;
	size_t nKeys;
};

struct GUIParams;
struct mInputMap;

void GBAGUIRemapKeys(struct GUIParams*, struct mInputMap*, const struct GUIInputKeys*);

#endif
