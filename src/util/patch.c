/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/patch.h>

#include <mgba-util/patch/ips.h>
#include <mgba-util/patch/ups.h>

bool loadPatch(struct VFile* vf, struct Patch* patch) {
	patch->vf = vf;

	if (loadPatchIPS(patch)) {
		return true;
	}

	if (loadPatchUPS(patch)) {
		return true;
	}

	patch->outputSize = 0;
	patch->applyPatch = 0;
	return false;
}
