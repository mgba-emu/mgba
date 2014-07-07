#include "util/patch.h"

#include "util/patch-ips.h"
#include "util/patch-ups.h"

int loadPatch(int patchfd, struct Patch* patch) {
	patch->patchfd = patchfd;

	if (loadPatchIPS(patch)) {
		return 1;
	}

	if (loadPatchUPS(patch)) {
		return 1;
	}

	patch->outputSize = 0;
	patch->applyPatch = 0;
	return 0;
}
