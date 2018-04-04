/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/patch/ips.h>

#include <mgba-util/crc32.h>
#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

enum {
	IN_CHECKSUM = -12,
	OUT_CHECKSUM = -8,
	PATCH_CHECKSUM = -4,
};

static size_t _UPSOutputSize(struct Patch* patch, size_t inSize);

static bool _UPSApplyPatch(struct Patch* patch, const void* in, size_t inSize, void* out, size_t outSize);
static bool _BPSApplyPatch(struct Patch* patch, const void* in, size_t inSize, void* out, size_t outSize);

static size_t _decodeLength(struct VFile* vf);

bool loadPatchUPS(struct Patch* patch) {
	patch->vf->seek(patch->vf, 0, SEEK_SET);

	char buffer[4];
	if (patch->vf->read(patch->vf, buffer, 4) != 4) {
		return false;
	}

	if (memcmp(buffer, "UPS1", 4) == 0) {
		patch->applyPatch = _UPSApplyPatch;
	} else if (memcmp(buffer, "BPS1", 4) == 0) {
		patch->applyPatch = _BPSApplyPatch;
	} else {
		return false;
	}

	size_t filesize = patch->vf->size(patch->vf);

	uint32_t goodCrc32;
	patch->vf->seek(patch->vf, PATCH_CHECKSUM, SEEK_END);
	if (patch->vf->read(patch->vf, &goodCrc32, 4) != 4) {
		return false;
	}

	uint32_t crc = fileCrc32(patch->vf, filesize + PATCH_CHECKSUM);
	if (crc != goodCrc32) {
		return false;
	}

	patch->outputSize = _UPSOutputSize;
	return true;
}

size_t _UPSOutputSize(struct Patch* patch, size_t inSize) {
	UNUSED(inSize);
	patch->vf->seek(patch->vf, 4, SEEK_SET);
	if (_decodeLength(patch->vf) != inSize) {
		return 0;
	}
	return _decodeLength(patch->vf);
}

bool _UPSApplyPatch(struct Patch* patch, const void* in, size_t inSize, void* out, size_t outSize) {
	// TODO: Input checksum

	size_t filesize = patch->vf->size(patch->vf);
	patch->vf->seek(patch->vf, 4, SEEK_SET);
	_decodeLength(patch->vf); // Discard input size
	if (_decodeLength(patch->vf) != outSize) {
		return false;
	}

	memcpy(out, in, inSize > outSize ? outSize : inSize);

	size_t offset = 0;
	size_t alreadyRead = 0;
	uint8_t* buf = out;
	while (alreadyRead < filesize + IN_CHECKSUM) {
		offset += _decodeLength(patch->vf);
		uint8_t byte;

		while (true) {
			if (patch->vf->read(patch->vf, &byte, 1) != 1) {
				return false;
			}
			if (offset >= outSize) {
				return false;
			}
			buf[offset] ^= byte;
			++offset;
			if (!byte) {
				break;
			}
		}
		alreadyRead = patch->vf->seek(patch->vf, 0, SEEK_CUR);
	}

	uint32_t goodCrc32;
	patch->vf->seek(patch->vf, OUT_CHECKSUM, SEEK_END);
	if (patch->vf->read(patch->vf, &goodCrc32, 4) != 4) {
		return false;
	}

	patch->vf->seek(patch->vf, 0, SEEK_SET);
	if (doCrc32(out, outSize) != goodCrc32) {
		return false;
	}
	return true;
}

bool _BPSApplyPatch(struct Patch* patch, const void* in, size_t inSize, void* out, size_t outSize) {
	patch->vf->seek(patch->vf, IN_CHECKSUM, SEEK_END);
	uint32_t expectedInChecksum;
	uint32_t expectedOutChecksum;
	patch->vf->read(patch->vf, &expectedInChecksum, sizeof(expectedInChecksum));
	patch->vf->read(patch->vf, &expectedOutChecksum, sizeof(expectedOutChecksum));

	uint32_t inputChecksum = doCrc32(in, inSize);
	uint32_t outputChecksum = 0;

	if (inputChecksum != expectedInChecksum) {
		return false;
	}

	ssize_t filesize = patch->vf->size(patch->vf);
	patch->vf->seek(patch->vf, 4, SEEK_SET);
	_decodeLength(patch->vf); // Discard input size
	if (_decodeLength(patch->vf) != outSize) {
		return false;
	}
	if (inSize > SSIZE_MAX || outSize > SSIZE_MAX) {
		return false;
	}
	size_t metadataLength = _decodeLength(patch->vf);
	patch->vf->seek(patch->vf, metadataLength, SEEK_CUR); // Skip metadata
	size_t writeLocation = 0;
	ssize_t readSourceLocation = 0;
	ssize_t readTargetLocation = 0;
	size_t readOffset;
	uint8_t* writeBuffer = out;
	const uint8_t* readBuffer = in;
	while (patch->vf->seek(patch->vf, 0, SEEK_CUR) < filesize + IN_CHECKSUM) {
		size_t command = _decodeLength(patch->vf);
		size_t length = (command >> 2) + 1;
		if (writeLocation + length > outSize) {
			return false;
		}
		size_t i;
		switch (command & 0x3) {
		case 0x0:
			// SourceRead
			memmove(&writeBuffer[writeLocation], &readBuffer[writeLocation], length);
			outputChecksum = crc32(outputChecksum, &writeBuffer[writeLocation], length);
			writeLocation += length;
			break;
		case 0x1:
			// TargetRead
			if (patch->vf->read(patch->vf, &writeBuffer[writeLocation], length) != (ssize_t) length) {
				return false;
			}
			outputChecksum = crc32(outputChecksum, &writeBuffer[writeLocation], length);
			writeLocation += length;
			break;
		case 0x2:
			// SourceCopy
			readOffset = _decodeLength(patch->vf);
			if (readOffset & 1) {
				readSourceLocation -= readOffset >> 1;
			} else {
				readSourceLocation += readOffset >> 1;
			}
			if (readSourceLocation < 0 || readSourceLocation > (ssize_t) inSize) {
				return false;
			}
			memmove(&writeBuffer[writeLocation], &readBuffer[readSourceLocation], length);
			outputChecksum = crc32(outputChecksum, &writeBuffer[writeLocation], length);
			writeLocation += length;
			readSourceLocation += length;
			break;
		case 0x3:
			// TargetCopy
			readOffset = _decodeLength(patch->vf);
			if (readOffset & 1) {
				readTargetLocation -= readOffset >> 1;
			} else {
				readTargetLocation += readOffset >> 1;
			}
			if (readTargetLocation < 0 || readTargetLocation > (ssize_t) outSize) {
				return false;
			}
			for (i = 0; i < length; ++i) {
				// This needs to be bytewise as it can overlap
				writeBuffer[writeLocation] = writeBuffer[readTargetLocation];
				++writeLocation;
				++readTargetLocation;
			}
			outputChecksum = crc32(outputChecksum, &writeBuffer[writeLocation - length], length);
			break;
		}
	}
	if (expectedOutChecksum != outputChecksum) {
		return false;
	}
	return true;
}

size_t _decodeLength(struct VFile* vf) {
	size_t shift = 1;
	size_t value = 0;
	uint8_t byte;
	while (true) {
		if (vf->read(vf, &byte, 1) != 1) {
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
