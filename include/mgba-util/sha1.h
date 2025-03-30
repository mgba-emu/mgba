/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Based on https://github.com/clibs/sha1
 */
#ifndef SHA1_H
#define SHA1_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct SHA1Context {
	uint32_t state[5];
	uint32_t count[2];
	unsigned char buffer[64];
};

void sha1Init(struct SHA1Context* ctx);
void sha1Update(struct SHA1Context* ctx, const void* input, size_t len);
void sha1Finalize(uint8_t digest[20], struct SHA1Context* ctx);

void sha1Buffer(const void* input, size_t len, uint8_t* result);

struct VFile;
bool sha1File(struct VFile* vf, uint8_t* result);

CXX_GUARD_END

#endif
