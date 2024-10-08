/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Based on https://github.com/Zunawe/md5-c
 * Originally released under the Unlicense */
#ifndef MD5_H
#define MD5_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct MD5Context {
	size_t size;         // Size of input in bytes
	uint32_t buffer[4];  // Current accumulation of hash
	uint8_t input[0x40];   // Input to be used in the next step
	uint8_t digest[0x10];  // Result of algorithm
};

void md5Init(struct MD5Context* ctx);
void md5Update(struct MD5Context* ctx, const void* input, size_t len);
void md5Finalize(struct MD5Context* ctx);

void md5Buffer(const void* input, size_t len, uint8_t* result);

struct VFile;
bool md5File(struct VFile* vf, uint8_t* result);

CXX_GUARD_END

#endif
