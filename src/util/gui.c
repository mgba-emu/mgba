/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui.h"

void GUIInit(struct GUIParams* params) {
	memset(params->inputHistory, 0, sizeof(params->inputHistory));
	strncpy(params->currentPath, params->basePath, PATH_MAX);
}

void GUIPollInput(struct GUIParams* params, uint32_t* newInputOut, uint32_t* heldInput) {
	uint32_t input = params->pollInput(&params->keyMap);
	uint32_t newInput = 0;
	for (int i = 0; i < GUI_INPUT_MAX; ++i) {
		if (input & (1 << i)) {
			++params->inputHistory[i];
		} else {
			params->inputHistory[i] = -1;
		}
		if (!params->inputHistory[i] || (params->inputHistory[i] >= 30 && !(params->inputHistory[i] % 6))) {
			newInput |= (1 << i);
		}
	}
	if (newInputOut) {
		*newInputOut = newInput;
	}
	if (heldInput) {
		*heldInput = input;
	}
}
