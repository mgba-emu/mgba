#include "util/patch.h"

#include "util/patch-ips.h"

int loadPatch(int patchfd, struct Patch* patch) {
	patch->patchfd = patchfd;

	if (loadPatchIPS(patch)) {
		return 1;
	}

	patch->outputSize = 0;
	patch->applyPatch = 0;
	return 0;
}
