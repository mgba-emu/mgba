#include "util/patch.h"

#include "util/patch-ips.h"
#include "util/patch-ups.h"

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
