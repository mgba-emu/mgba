#include "util/patch-ips.h"

#include "util/crc32.h"
#include "util/patch.h"

#include <stdint.h>
#include <unistd.h>

enum {
	IN_CHECKSUM = -12,
	OUT_CHECKSUM = -8,
	PATCH_CHECKSUM = -4,

	BUFFER_SIZE = 1024
};

static size_t _UPSOutputSize(struct Patch* patch, size_t inSize);
static int _UPSApplyPatch(struct Patch* patch, void* out, size_t outSize);
static size_t _UPSDecodeLength(int fd);

int loadPatchUPS(struct Patch* patch) {
	lseek(patch->patchfd, 0, SEEK_SET);

	char buffer[BUFFER_SIZE];
	if (read(patch->patchfd, buffer, 4) != 4) {
		return 0;
	}

	if (memcmp(buffer, "UPS1", 4) != 0) {
		return 0;
	}

	size_t filesize = lseek(patch->patchfd, 0, SEEK_END);

	uint32_t goodCrc32;
	lseek(patch->patchfd, PATCH_CHECKSUM, SEEK_END);
	if (read(patch->patchfd, &goodCrc32, 4) != 4) {
		return 0;
	}

	size_t blocksize;
	size_t alreadyRead = 0;
	lseek(patch->patchfd, 0, SEEK_SET);
	uint32_t crc = 0;
	while (alreadyRead < filesize + PATCH_CHECKSUM) {
		size_t toRead = sizeof(buffer);
		if (toRead + alreadyRead > filesize + PATCH_CHECKSUM) {
			toRead = filesize + PATCH_CHECKSUM - alreadyRead;
		}
		blocksize = read(patch->patchfd, buffer, toRead);
		alreadyRead += blocksize;
		crc = updateCrc32(crc, buffer, blocksize);
		if (blocksize < toRead) {
			return 0;
		}
	}

	if (crc != goodCrc32) {
		return 0;
	}

	patch->outputSize = _UPSOutputSize;
	patch->applyPatch = _UPSApplyPatch;
	return 1;
}

size_t _UPSOutputSize(struct Patch* patch, size_t inSize) {
	(void) (inSize);
	lseek(patch->patchfd, 4, SEEK_SET);
	if (_UPSDecodeLength(patch->patchfd) != inSize) {
		return 0;
	}
	return _UPSDecodeLength(patch->patchfd);
}

int _UPSApplyPatch(struct Patch* patch, void* out, size_t outSize) {
	// TODO: Input checksum

	size_t filesize = lseek(patch->patchfd, 0, SEEK_END);
	lseek(patch->patchfd, 4, SEEK_SET);
	_UPSDecodeLength(patch->patchfd); // Discard input size
	if (_UPSDecodeLength(patch->patchfd) != outSize) {
		return 0;
	}

	size_t offset = 0;
	size_t alreadyRead = 0;
	uint8_t* buf = out;
	while (alreadyRead < filesize + IN_CHECKSUM) {
		offset += _UPSDecodeLength(patch->patchfd);
		uint8_t byte;

		while (1) {
			if (read(patch->patchfd, &byte, 1) != 1) {
				return 0;
			}
			buf[offset] ^= byte;
			++offset;
			if (!byte) {
				break;
			}
		}
		alreadyRead = lseek(patch->patchfd, 0, SEEK_CUR);
	}

	uint32_t goodCrc32;
	lseek(patch->patchfd, OUT_CHECKSUM, SEEK_END);
	if (read(patch->patchfd, &goodCrc32, 4) != 4) {
		return 0;
	}

	lseek(patch->patchfd, 0, SEEK_SET);
	if (crc32(out, outSize) != goodCrc32) {
		return 0;
	}
	return 1;
}

size_t _UPSDecodeLength(int fd) {
	size_t shift = 1;
	size_t value = 0;
	uint8_t byte;
	while (1) {
		if (read(fd, &byte, 1) != 1) {
			break;
		}
		value += (byte & 0x7f) * shift;
		if (byte & 0x80) {
			break;
		}
		shift <<= 7;
		value += shift;
	}
	return value;
}
