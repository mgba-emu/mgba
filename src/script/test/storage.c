/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>
#include <mgba/script.h>

#include "script/test.h"

#define SETUP_LUA \
	struct mScriptContext context; \
	mScriptContextInit(&context); \
	struct mScriptEngineContext* lua = mScriptContextRegisterEngine(&context, mSCRIPT_ENGINE_LUA); \
	mScriptContextAttachStdlib(&context); \
	mScriptContextAttachStorage(&context); \
	char bucketPath[PATH_MAX]; \
	mScriptStorageGetBucketPath("xtest", bucketPath); \
	remove(bucketPath)

M_TEST_SUITE_SETUP(mScriptStorage) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptStorage) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(basicInt) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 1");
	TEST_PROGRAM("assert(bucket.a == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicFloat) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 0.5");
	TEST_PROGRAM("assert(bucket.a == 0.5)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicBool) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = true");
	TEST_PROGRAM("assert(bucket.a == true)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicNil) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = nil");
	TEST_PROGRAM("assert(bucket.a == nil)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 'hello'");
	TEST_PROGRAM("assert(bucket.a == 'hello')");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicList) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = {1}");
	TEST_PROGRAM("assert(#bucket.a == 1)");
	TEST_PROGRAM("assert(bucket.a[1] == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(basicTable) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = {['a']=1}");
	TEST_PROGRAM("assert(#bucket.a == 1)");
	TEST_PROGRAM("assert(bucket.a.a == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(nullByteString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM("bucket.a = 'a\\x00b'");
	TEST_PROGRAM("assert(bucket.a == 'a\\x00b')");
	TEST_PROGRAM("assert(#bucket.a == 3)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(structured) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM(
		"bucket.a = {\n"
		"	['a'] = 1,\n"
		"	['b'] = {1},\n"
		"	['c'] = {\n"
		"		['d'] = 1\n"
		"	}\n"
		"}"
	);
	TEST_PROGRAM("assert(bucket.a)");
	TEST_PROGRAM("assert(bucket.a.a == 1)");
	TEST_PROGRAM("assert(#bucket.a.b == 1)");
	TEST_PROGRAM("assert(bucket.a.b[1] == 1)");
	TEST_PROGRAM("assert(#bucket.a.c == 1)");
	TEST_PROGRAM("assert(bucket.a.c.d == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(invalidObject) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	LOAD_PROGRAM("bucket.a = bucket");
	assert_false(lua->run(lua));

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeInt) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = 1");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":1\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeFloat) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = 0.5");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":0.5\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeBool) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = true");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":true\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeNil) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = nil");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":null\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = 'hello'");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":\"hello\"\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeList) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = {1, 2}");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":[\n\t\t1,\n\t\t2\n\t]\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeTable) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = {['b']=1}");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":{\n\t\t\"b\":1\n\t}\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(serializeNullByteString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("bucket.a = 'a\\x00b'");
	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));
	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	ssize_t size = vf->size(vf);
	char* buf = calloc(1, size + 1);
	assert_int_equal(vf->read(vf, buf, size), size);
	assert_string_equal(buf, "{\n\t\"a\":\"a\\u0000b\"\n}");
	free(buf);
	vf->close(vf);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeInt) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":1}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(bucket.a == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeFloat) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":0.5}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(bucket.a == 0.5)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeBool) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":true}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(bucket.a == true)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeNil) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":null}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(bucket.a == nil)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":\"hello\"}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(bucket.a == 'hello')");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeList) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":[1,2]}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(#bucket.a == 2)");
	TEST_PROGRAM("assert(bucket.a[1] == 1)");
	TEST_PROGRAM("assert(bucket.a[2] == 2)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeTable) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":{\"b\":1}}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(bucket.a.b == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeNullByteString) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{\"a\":\"a\\u0000b\"}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(bucket.a == 'a\\x00b')");
	TEST_PROGRAM("assert(bucket.a ~= 'a\\x00c')");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(deserializeError) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");

	TEST_PROGRAM("assert(not bucket.a)");

	static const char* json = "{a:1}";
	struct VFile* vf = VFileFromConstMemory(json, strlen(json));
	assert_false(mScriptStorageLoadBucketVF(&context, "xtest", vf));
	TEST_PROGRAM("assert(not bucket.a)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(structuredRoundTrip) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");
	TEST_PROGRAM(
		"bucket.a = {\n"
		"	['a'] = 1,\n"
		"	['b'] = {1},\n"
		"	['c'] = {\n"
		"		['d'] = 1\n"
		"	}\n"
		"}"
	);

	struct VFile* vf = VFileOpen("test.json", O_CREAT | O_TRUNC | O_WRONLY);
	assert_true(mScriptStorageSaveBucketVF(&context, "xtest", vf));

	TEST_PROGRAM("bucket.a = nil")
	TEST_PROGRAM("assert(not bucket.a)");

	vf = VFileOpen("test.json", O_RDONLY);
	assert_non_null(vf);
	assert_true(mScriptStorageLoadBucketVF(&context, "xtest", vf));

	TEST_PROGRAM("assert(bucket.a)");
	TEST_PROGRAM("assert(bucket.a.a == 1)");
	TEST_PROGRAM("assert(#bucket.a.b == 1)");
	TEST_PROGRAM("assert(bucket.a.b[1] == 1)");
	TEST_PROGRAM("assert(#bucket.a.c == 1)");
	TEST_PROGRAM("assert(bucket.a.c.d == 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(autoflush) {
	SETUP_LUA;

	TEST_PROGRAM("bucket = storage:getBucket('xtest')");
	TEST_PROGRAM("assert(bucket)");
	TEST_PROGRAM("assert(not bucket.a)");

	TEST_PROGRAM("bucket:enableAutoFlush(true)")
	TEST_PROGRAM("bucket.a = 1");
	TEST_PROGRAM("storage:flushAll()");
	TEST_PROGRAM("assert(bucket:reload())")
	TEST_PROGRAM("assert(bucket.a == 1)");

	TEST_PROGRAM("bucket:enableAutoFlush(false)")
	TEST_PROGRAM("bucket.a = 2");
	TEST_PROGRAM("storage:flushAll()");
	TEST_PROGRAM("assert(bucket:reload())")
	TEST_PROGRAM("assert(bucket.a == 1)");

	TEST_PROGRAM("bucket:enableAutoFlush(false)")
	TEST_PROGRAM("bucket.a = 3");
	TEST_PROGRAM("storage:flushAll()");
	TEST_PROGRAM("bucket:enableAutoFlush(true)")
	TEST_PROGRAM("storage:flushAll()");
	TEST_PROGRAM("assert(bucket:reload())")
	TEST_PROGRAM("assert(bucket.a == 3)");

	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptStorage,
	cmocka_unit_test(basicInt),
	cmocka_unit_test(basicFloat),
	cmocka_unit_test(basicBool),
	cmocka_unit_test(basicNil),
	cmocka_unit_test(basicString),
	cmocka_unit_test(basicList),
	cmocka_unit_test(basicTable),
	cmocka_unit_test(nullByteString),
	cmocka_unit_test(invalidObject),
	cmocka_unit_test(structured),
	cmocka_unit_test(serializeInt),
	cmocka_unit_test(serializeFloat),
	cmocka_unit_test(serializeBool),
	cmocka_unit_test(serializeNil),
	cmocka_unit_test(serializeString),
	cmocka_unit_test(serializeList),
	cmocka_unit_test(serializeTable),
	cmocka_unit_test(serializeNullByteString),
	cmocka_unit_test(deserializeInt),
	cmocka_unit_test(deserializeFloat),
	cmocka_unit_test(deserializeBool),
	cmocka_unit_test(deserializeNil),
	cmocka_unit_test(deserializeString),
	cmocka_unit_test(deserializeList),
	cmocka_unit_test(deserializeTable),
	cmocka_unit_test(deserializeNullByteString),
	cmocka_unit_test(deserializeError),
	cmocka_unit_test(structuredRoundTrip),
	cmocka_unit_test(autoflush),
)
