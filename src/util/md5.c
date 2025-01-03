/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Based on https://github.com/Zunawe/md5-c
 * Originally released under the Unlicense
 *
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 */
#include <mgba-util/md5.h>

#include <mgba-util/vfs.h>

/*
 * Constants defined by the MD5 algorithm
 */
#define A 0x67452301
#define B 0xEFCDAB89
#define C 0x98BADCFE
#define D 0x10325476

static const uint32_t S[] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                              5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

static const uint32_t K[] = { 0xD76AA478, 0xE8C7B756, 0x242070DB, 0xC1BDCEEE,
                              0xF57C0FAF, 0x4787C62A, 0xA8304613, 0xFD469501,
                              0x698098D8, 0x8B44F7AF, 0xFFFF5BB1, 0x895CD7BE,
                              0x6B901122, 0xFD987193, 0xA679438E, 0x49B40821,
                              0xF61E2562, 0xC040B340, 0x265E5A51, 0xE9B6C7AA,
                              0xD62F105D, 0x02441453, 0xD8A1E681, 0xE7D3FBC8,
                              0x21E1CDE6, 0xC33707D6, 0xF4D50D87, 0x455A14ED,
                              0xA9E3E905, 0xFCEFA3F8, 0x676F02D9, 0x8D2A4C8A,
                              0xFFFA3942, 0x8771F681, 0x6D9D6122, 0xFDE5380C,
                              0xA4BEEA44, 0x4BDECFA9, 0xF6BB4B60, 0xBEBFBC70,
                              0x289B7EC6, 0xEAA127FA, 0xD4EF3085, 0x04881D05,
                              0xD9D4D039, 0xE6DB99E5, 0x1FA27CF8, 0xC4AC5665,
                              0xF4292244, 0x432AFF97, 0xAB9423A7, 0xFC93A039,
                              0x655B59C3, 0x8F0CCC92, 0xFFEFF47D, 0x85845DD1,
                              0x6FA87E4F, 0xFE2CE6E0, 0xA3014314, 0x4E0811A1,
                              0xF7537E82, 0xBD3AF235, 0x2AD7D2BB, 0xEB86D391 };

/*
 * Padding used to make the size (in bits) of the input congruent to 448 mod 512
 */
static const uint8_t PADDING[] = { 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

/*
 * Bit-manipulation functions defined by the MD5 algorithm
 */
#define F(X, Y, Z) (((X) & (Y)) | (~(X) & (Z)))
#define G(X, Y, Z) (((X) & (Z)) | ((Y) & ~(Z)))
#define H(X, Y, Z) ((X) ^ (Y) ^ (Z))
#define I(X, Y, Z) ((Y) ^ ((X) | ~(Z)))

/*
 * Rotates a 32-bit word left by n bits
 */
static uint32_t rotateLeft(uint32_t x, uint32_t n) {
	return (x << n) | (x >> (32 - n));
}

/*
 * Step on 512 bits of input with the main MD5 algorithm.
 */
static void md5Step(uint32_t* buffer, const uint32_t* input) {
	uint32_t AA = buffer[0];
	uint32_t BB = buffer[1];
	uint32_t CC = buffer[2];
	uint32_t DD = buffer[3];

	uint32_t E;

	unsigned j;

	for (unsigned i = 0; i < 64; ++i) {
		switch (i / 16) {
		case 0:
			E = F(BB, CC, DD);
			j = i & 0xF;
			break;
		case 1:
			E = G(BB, CC, DD);
			j = ((i * 5) + 1) & 0xF;
			break;
		case 2:
			E = H(BB, CC, DD);
			j = ((i * 3) + 5) & 0xF;
			break;
		default:
			E = I(BB, CC, DD);
			j = (i * 7) & 0xF;
			break;
		}

		uint32_t temp = DD;
		DD = CC;
		CC = BB;
		BB += rotateLeft(AA + E + K[i] + input[j], S[i]);
		AA = temp;
	}

	buffer[0] += AA;
	buffer[1] += BB;
	buffer[2] += CC;
	buffer[3] += DD;
}

/*
 * Initialize a context
 */
void md5Init(struct MD5Context* ctx) {
	memset(ctx, 0, sizeof(*ctx));

	ctx->buffer[0] = A;
	ctx->buffer[1] = B;
	ctx->buffer[2] = C;
	ctx->buffer[3] = D;
}

/*
 * Add some amount of input to the context
 *
 * If the input fills out a block of 512 bits, apply the algorithm (md5Step)
 * and save the result in the buffer. Also updates the overall size.
 */
void md5Update(struct MD5Context* ctx, const void* input, size_t len) {
	uint32_t buffer[16];
	unsigned offset = ctx->size & 0x3F;
	const uint8_t* inputBuffer = input;
	ctx->size += len;

	// Copy each byte in input_buffer into the next space in our context input
	unsigned i;
	for (i = 0; i < len; ++i) {
		ctx->input[offset] = inputBuffer[i];

		// If we've filled our context input, copy it into our local array input
		// then reset the offset to 0 and fill in a new buffer.
		// Every time we fill out a chunk, we run it through the algorithm
		// to enable some back and forth between cpu and i/o
		if (offset < 0x3F) {
			++offset;
			continue;
		}

		unsigned j;
		for (j = 0; j < 16; ++j) {
			// Convert to little-endian
			// The local variable `input` our 512-bit chunk separated into 32-bit words
			// we can use in calculations
			LOAD_32LE(buffer[j], j * 4, ctx->input);
		}
		md5Step(ctx->buffer, buffer);
		offset = 0;
	}
}

/*
 * Pad the current input to get to 448 bits, append the size in bits to the very end,
 * and save the result of the final iteration into digest.
 */
void md5Finalize(struct MD5Context* ctx) {
	uint32_t input[16];
	int offset = ctx->size & 0x3F;
	unsigned paddingLength = offset < 56 ? 56 - offset : (56 + 64) - offset;

	// Fill in the padding and undo the changes to size that resulted from the update
	md5Update(ctx, PADDING, paddingLength);
	ctx->size -= paddingLength;

	// Do a final update (internal to this function)
	// Last two 32-bit words are the two halves of the size (converted from bytes to bits)
	unsigned j;
	for (j = 0; j < 14; ++j) {
		LOAD_32LE(input[j], j * 4, ctx->input);
	}
	input[14] = (uint32_t) (ctx->size * 8);
	input[15] = (uint32_t) ((ctx->size * 8ULL) >> 32);

	md5Step(ctx->buffer, input);

	// Move the result into digest (convert from little-endian)
	unsigned i;
	for (i = 0; i < 4; ++i) {
		STORE_32LE(ctx->buffer[i], i * 4, ctx->digest);
	}
}

void md5Buffer(const void* input, size_t len, uint8_t* result) {
	struct MD5Context ctx;
	md5Init(&ctx);
	md5Update(&ctx, input, len);
	md5Finalize(&ctx);
	memcpy(result, ctx.digest, sizeof(ctx.digest));
}

bool md5File(struct VFile* vf, uint8_t* result) {
	struct MD5Context ctx;
	uint8_t buffer[2048];
	md5Init(&ctx);

	ssize_t read;
	ssize_t position = vf->seek(vf, 0, SEEK_CUR);
	if (vf->seek(vf, 0, SEEK_SET) < 0) {
		return false;
	}
	while ((read = vf->read(vf, buffer, sizeof(buffer))) > 0) {
		md5Update(&ctx, buffer, read);
	}
	vf->seek(vf, position, SEEK_SET);
	if (read < 0) {
		return false;
	}
	md5Finalize(&ctx);
	memcpy(result, ctx.digest, sizeof(ctx.digest));
	return true;
}
