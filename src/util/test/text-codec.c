/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/text-codec.h>
#include <mgba-util/vfs.h>

M_TEST_DEFINE(emptyCodec) {
	struct VFile* vf = VFileMemChunk(NULL, 0);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, true));
	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(singleEntry) {
	static const char file[] = "41=B";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, false));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "BB", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "BB", 2);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(singleEntryReverse) {
	static const char file[] = "41=B";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, true));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'B', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "A", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'B', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'B', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "AA", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 'B', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "A", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'B', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "A", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'B', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 'B', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "AA", 2);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(twoEntry) {
	static const char file[] =
		"41=B\n"
		"43=D";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, false));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "D", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "BB", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "DD", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "BD", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "DB", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "D", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "D", 1);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(longEntry) {
	static const char file[] =
		"01=Ab\n"
		"02=cd";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, false));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "cd", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "AbAb", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "cdcd", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "Abcd", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "cdAb", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "cd", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "cd", 2);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(longEntryReverse) {
	static const char file[] =
		"01=Ab\n"
		"02=cd";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, true));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\1", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\2", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\1\1", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\2\2", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\1\2", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\2\1", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 'e', output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\1", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 'e', output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\2", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'e', output + len, sizeof(output) - len), -1);
	assert_int_equal(TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 0);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'e', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\1", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'e', output + len, sizeof(output) - len), -1);
	assert_int_equal(TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 0);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'e', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\2", 1);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(overlappingEntry) {
	static const char file[] =
		"FF01=Ab\n"
		"FF02=Ac";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, false));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ac", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ac", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "AbAb", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "AcAc", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "AbAc", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "AcAb", 4);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	assert_int_equal(TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 0);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0xFF, output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "Ab", 2);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(overlappingEntryReverse) {
	static const char file[] =
		"FF01=Ab\n"
		"FF02=Ac";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, true));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\xFF\1", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\xFF\1", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\xFF\2", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\xFF\2", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "\xFF\1\xFF\1", 4);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "\xFF\2\xFF\2", 4);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "\xFF\1\xFF\2", 4);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'c', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 4);
	assert_memory_equal(output, "\xFF\2\xFF\1", 4);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	assert_int_equal(TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\xFF\1", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len), -1);
	assert_int_equal(TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 0);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len), -1);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\xFF\1", 2);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'b', output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 'A', output + len, sizeof(output) - len);
	assert_int_equal(TextCodecAdvance(&iter, 'd', output + len, sizeof(output) - len), -1);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "\xFF\1", 2);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(raggedEntry) {
	static const char file[] =
		"4142=bc\n"
		"41=B\n"
		"42=C";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, false));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "BB", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "CB", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "bc", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 3);
	assert_memory_equal(output, "bcB", 3);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 3);
	assert_memory_equal(output, "Bbc", 3);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 3);
	assert_memory_equal(output, "bcC", 3);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0x41, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x43, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0x42, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "BC", 2);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(controlCodes) {
	static const char file[] =
		"*=01\n"
		"/=02\n"
		"\\=03";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, true));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\n", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 2, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\x1F", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 3, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\x1E", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, '\n', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\1", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, '\x1F', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\2", 1);

	len = 0;
	TextCodecStartEncode(&codec, &iter);
	len += TextCodecAdvance(&iter, '\x1E', output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "\3", 1);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_DEFINE(nullBytes) {
	static const char file[] =
		"00=A\n"
		"0000=a\n"
		"0001=b\n"
		"01=B\n"
		"0100=c";
	struct VFile* vf = VFileFromConstMemory(file, sizeof(file) - 1);
	struct TextCodec codec;
	assert_true(TextCodecLoadTBL(&codec, vf, false));
	struct TextCodecIterator iter;
	uint8_t output[16] = {};
	size_t len;

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "A", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "a", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "aA", 2);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "b", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "B", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 1);
	assert_memory_equal(output, "c", 1);

	len = 0;
	TextCodecStartDecode(&codec, &iter);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 0, output + len, sizeof(output) - len);
	len += TextCodecAdvance(&iter, 1, output + len, sizeof(output) - len);
	len += TextCodecFinish(&iter, output + len, sizeof(output) - len);
	assert_int_equal(len, 2);
	assert_memory_equal(output, "cB", 2);

	TextCodecDeinit(&codec);
	vf->close(vf);
}

M_TEST_SUITE_DEFINE(TextCodec,
	cmocka_unit_test(emptyCodec),
	cmocka_unit_test(singleEntry),
	cmocka_unit_test(singleEntryReverse),
	cmocka_unit_test(twoEntry),
	cmocka_unit_test(longEntry),
	cmocka_unit_test(longEntryReverse),
	cmocka_unit_test(overlappingEntry),
	cmocka_unit_test(overlappingEntryReverse),
	cmocka_unit_test(raggedEntry),
	cmocka_unit_test(controlCodes),
	cmocka_unit_test(nullBytes))
