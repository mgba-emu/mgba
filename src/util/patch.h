#ifndef PATCH_H
#define PATCH_H

#include "util/common.h"

struct VFile;

struct Patch {
	struct VFile* vf;

	size_t (*outputSize)(struct Patch* patch, size_t inSize);
	bool (*applyPatch)(struct Patch* patch, void* out, size_t outSize);
};

bool loadPatch(struct VFile* vf, struct Patch* patch);

#endif
