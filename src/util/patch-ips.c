#include "util/patch-ips.h"

#include "util/patch.h"

static size_t _IPSOutputSize(struct Patch* patch, size_t inSize);
static int _IPSApplyPatch(struct Patch* patch, void* out, size_t outSize);

int loadPatchIPS(struct Patch* patch) {
	lseek(patch->patchfd, 0, SEEK_SET);

	char buffer[5];
	if (read(patch->patchfd, buffer, 5) != 5) {
		return 0;
	}

	if (memcmp(buffer, "PATCH", 5) != 0) {
		return 0;
	}

	lseek(patch->patchfd, -3, SEEK_END);
	if (read(patch->patchfd, buffer, 3) != 3) {
		return 0;
	}

	if (memcmp(buffer, "EOF", 3) != 0) {
		return 0;
	}

	patch->outputSize = _IPSOutputSize;
	patch->applyPatch = _IPSApplyPatch;
	return 1;
}

size_t _IPSOutputSize(struct Patch* patch, size_t inSize) {
	UNUSED(patch);
	return inSize;
}

int _IPSApplyPatch(struct Patch* patch, void* out, size_t outSize) {
	if (lseek(patch->patchfd, 5, SEEK_SET) != 5) {
		return 0;
	}
	uint8_t* buf = out;

	while (1) {
		uint32_t offset = 0;
		uint16_t size = 0;

		if (read(patch->patchfd, &offset, 3) != 3) {
			return 0;
		}

		if (offset == 0x464F45) {
			return 1;
		}

		offset = (offset >> 16) | (offset & 0xFF00) | ((offset << 16) & 0xFF0000);
		if (read(patch->patchfd, &size, 2) != 2) {
			return 0;
		}
		if (!size) {
			// RLE chunk
			if (read(patch->patchfd, &size, 2) != 2) {
				return 0;
			}
			size = (size >> 8) | (size << 8);
			uint8_t byte;
			if (read(patch->patchfd, &byte, 1) != 1) {
				return 0;
			}
			if (offset + size > outSize) {
				return 0;
			}
			memset(&buf[offset], byte, size);
		} else {
			size = (size >> 8) | (size << 8);
			if (offset + size > outSize) {
				return 0;
			}
			if (read(patch->patchfd, &buf[offset], size) != size) {
				return 0;
			}
		}
	}
}
