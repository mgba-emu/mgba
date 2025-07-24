/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/vfs.h>

#ifdef ENABLE_VFS
M_TEST_DEFINE(openNullPathR) {
	struct VFile* vf = VFileOpen(NULL, O_RDONLY);
	assert_null(vf);
}

M_TEST_DEFINE(openNullPathW) {
	struct VFile* vf = VFileOpen(NULL, O_WRONLY);
	assert_null(vf);
}

M_TEST_DEFINE(openNullPathCreate) {
	struct VFile* vf = VFileOpen(NULL, O_CREAT);
	assert_null(vf);
}

M_TEST_DEFINE(openNullPathWCreate) {
	struct VFile* vf = VFileOpen(NULL, O_WRONLY | O_CREAT);
	assert_null(vf);
}
#endif

M_TEST_DEFINE(openNullMem0) {
	struct VFile* vf = VFileFromMemory(NULL, 0);
	assert_null(vf);
}

M_TEST_DEFINE(openNullMemNonzero) {
	struct VFile* vf = VFileFromMemory(NULL, 32);
	assert_null(vf);
}

M_TEST_DEFINE(openNullConstMem0) {
	struct VFile* vf = VFileFromConstMemory(NULL, 0);
	assert_null(vf);
}

M_TEST_DEFINE(openNullConstMemNonzero) {
	struct VFile* vf = VFileFromConstMemory(NULL, 32);
	assert_null(vf);
}

M_TEST_DEFINE(openNullMemChunk0) {
	struct VFile* vf = VFileMemChunk(NULL, 0);
	assert_non_null(vf);
	assert_int_equal(vf->size(vf), 0);
	vf->close(vf);
}

M_TEST_DEFINE(openNonNullMemChunk0) {
	const uint8_t bytes[32] = {};
	struct VFile* vf = VFileMemChunk(bytes, 0);
	assert_non_null(vf);
	assert_int_equal(vf->size(vf), 0);
	vf->close(vf);
}

M_TEST_DEFINE(openNullMemChunkNonzero) {
	struct VFile* vf = VFileMemChunk(NULL, 32);
	assert_non_null(vf);
	assert_int_equal(vf->size(vf), 32);
	vf->close(vf);
}

M_TEST_DEFINE(resizeMem) {
	uint8_t bytes[32] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
	};
	struct VFile* vf = VFileFromMemory(bytes, 32);
	assert_non_null(vf);
	assert_int_equal(vf->seek(vf, 0, SEEK_END), 32);
	assert_int_equal(vf->size(vf), 32);

	vf->truncate(vf, 64);
	assert_int_equal(vf->size(vf), 32);
	assert_int_equal(bytes[15], 0x0F);
	assert_int_equal(bytes[16], 0x10);
	assert_int_equal(vf->seek(vf, 0, SEEK_CUR), 32);

	vf->truncate(vf, 16);
	assert_int_equal(vf->size(vf), 16);
	assert_int_equal(vf->seek(vf, 0, SEEK_CUR), 16);

	vf->truncate(vf, 64);
	assert_int_equal(vf->size(vf), 32);
	assert_int_equal(bytes[15], 0x0F);
	assert_int_equal(bytes[16], 0x00);
	assert_int_equal(vf->seek(vf, 0, SEEK_CUR), 16);

	vf->close(vf);
}

M_TEST_DEFINE(resizeConstMem) {
	uint8_t bytes[32] = {0};
	struct VFile* vf = VFileFromConstMemory(bytes, 32);
	assert_non_null(vf);
	assert_int_equal(vf->seek(vf, 0, SEEK_END), 32);
	assert_int_equal(vf->size(vf), 32);

	vf->truncate(vf, 64);
	assert_int_equal(vf->size(vf), 32);
	assert_int_equal(vf->seek(vf, 0, SEEK_CUR), 32);

	vf->truncate(vf, 16);
	assert_int_equal(vf->size(vf), 32);
	assert_int_equal(vf->seek(vf, 0, SEEK_CUR), 32);
	vf->close(vf);
}

M_TEST_DEFINE(resizeMemChunk) {
	uint8_t bytes[32] = {0};
	struct VFile* vf = VFileMemChunk(bytes, 32);
	assert_non_null(vf);
	assert_int_equal(vf->seek(vf, 0, SEEK_END), 32);
	assert_int_equal(vf->size(vf), 32);

	vf->truncate(vf, 64);
	assert_int_equal(vf->size(vf), 64);
	assert_int_equal(vf->seek(vf, 0, SEEK_CUR), 32);

	vf->truncate(vf, 16);
	assert_int_equal(vf->size(vf), 16);
	assert_int_equal(vf->seek(vf, 0, SEEK_CUR), 16);
	vf->close(vf);
}

M_TEST_DEFINE(mapMem) {
	uint8_t bytes[32] = "Test Pattern";
	struct VFile* vf = VFileFromMemory(bytes, 32);
	assert_non_null(vf);
	void* mapped = vf->map(vf, 32, MAP_READ);
	assert_non_null(mapped);
	assert_ptr_equal(mapped, &bytes);
	vf->unmap(vf, mapped, 32);
	vf->close(vf);
}

M_TEST_DEFINE(mapConstMem) {
	uint8_t bytes[32] = "Test Pattern";
	struct VFile* vf = VFileFromConstMemory(bytes, 32);
	assert_non_null(vf);
	void* mapped = vf->map(vf, 32, MAP_READ);
	assert_non_null(mapped);
	assert_ptr_equal(mapped, &bytes);
	vf->unmap(vf, mapped, 32);
	vf->close(vf);
}

M_TEST_DEFINE(mapMemChunk) {
	uint8_t bytes[32] = "Test Pattern";
	struct VFile* vf = VFileMemChunk(bytes, 32);
	assert_non_null(vf);
	void* mapped = vf->map(vf, 32, MAP_READ);
	assert_non_null(mapped);
	assert_ptr_not_equal(mapped, &bytes);
	assert_memory_equal(mapped, &bytes, 32);
	vf->write(vf, bytes, sizeof(bytes));
	void* mapped2 = vf->map(vf, 32, MAP_READ);
	assert_ptr_equal(mapped, mapped2);
	vf->unmap(vf, mapped, 32);
	vf->unmap(vf, mapped2, 32);
	vf->close(vf);
}

M_TEST_SUITE_DEFINE(VFS,
#ifdef ENABLE_VFS
	cmocka_unit_test(openNullPathR),
	cmocka_unit_test(openNullPathW),
	cmocka_unit_test(openNullPathCreate),
	cmocka_unit_test(openNullPathWCreate),
#endif
	cmocka_unit_test(openNullMem0),
	cmocka_unit_test(openNullMemNonzero),
	cmocka_unit_test(openNullConstMem0),
	cmocka_unit_test(openNullConstMemNonzero),
	cmocka_unit_test(openNullMemChunk0),
	cmocka_unit_test(openNonNullMemChunk0),
	cmocka_unit_test(openNullMemChunkNonzero),
	cmocka_unit_test(resizeMem),
	cmocka_unit_test(resizeConstMem),
	cmocka_unit_test(resizeMemChunk),
	cmocka_unit_test(mapMem),
	cmocka_unit_test(mapConstMem),
	cmocka_unit_test(mapMemChunk))
