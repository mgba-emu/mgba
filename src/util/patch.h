#ifndef PATCH_H
#define PATCH_H

#include "common.h"

struct Patch {
	int patchfd;

	size_t (*outputSize)(struct Patch* patch, size_t inSize);
	int (*applyPatch)(struct Patch* patch, void* out, size_t outSize);
};

int loadPatch(int patchfd, struct Patch* patch);

#endif
