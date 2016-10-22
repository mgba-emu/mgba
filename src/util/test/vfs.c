/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include "util/vfs.h"

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
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

M_TEST_SUITE_DEFINE(VFS,
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
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
	cmocka_unit_test(openNullMemChunkNonzero))
