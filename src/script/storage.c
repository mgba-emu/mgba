/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/storage.h>

#define STORAGE_LEN_MAX 64

struct mScriptStorageBucket {
	char* name;
	struct mScriptValue* root;
	bool dirty;
};

struct mScriptStorageContext {
	struct Table buckets;
};

void mScriptStorageBucketDeinit(void*);
struct mScriptValue* mScriptStorageBucketGet(struct mScriptStorageBucket* adapter, const char* key);
static void mScriptStorageBucketSet(struct mScriptStorageBucket* adapter, const char* key, struct mScriptValue* value);
static void mScriptStorageBucketSetVoid(struct mScriptStorageBucket* adapter, const char* key, struct mScriptValue* value);
static void mScriptStorageBucketSetSInt(struct mScriptStorageBucket* adapter, const char* key, int64_t value);
static void mScriptStorageBucketSetUInt(struct mScriptStorageBucket* adapter, const char* key, uint64_t value);
static void mScriptStorageBucketSetFloat(struct mScriptStorageBucket* adapter, const char* key, double value);
static void mScriptStorageBucketSetBool(struct mScriptStorageBucket* adapter, const char* key, bool value);

void mScriptStorageContextDeinit(struct mScriptStorageContext*);
struct mScriptStorageBucket* mScriptStorageGetBucket(struct mScriptStorageContext*, const char* name);

mSCRIPT_DECLARE_STRUCT(mScriptStorageBucket);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptStorageBucket, WRAPPER, _get, mScriptStorageBucketGet, 1, CHARP, key);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setSInt, mScriptStorageBucketSetSInt, 2, CHARP, key, S64, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setUInt, mScriptStorageBucketSetUInt, 2, CHARP, key, U64, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setFloat, mScriptStorageBucketSetFloat, 2, CHARP, key, F64, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setBool, mScriptStorageBucketSetBool, 2, CHARP, key, BOOL, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setStr, mScriptStorageBucketSet, 2, CHARP, key, WSTR, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setList, mScriptStorageBucketSet, 2, CHARP, key, WLIST, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setTable, mScriptStorageBucketSet, 2, CHARP, key, WTABLE, value);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, setVoid, mScriptStorageBucketSetVoid, 2, CHARP, key, NUL, value);

mSCRIPT_DEFINE_STRUCT(mScriptStorageBucket)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setSInt)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setUInt)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setFloat)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setBool)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setStr)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setList)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setTable)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setVoid)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_GET(mScriptStorageBucket)
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT(mScriptStorageContext);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageContext, _deinit, mScriptStorageContextDeinit, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptStorageContext, S(mScriptStorageBucket), getBucket, mScriptStorageGetBucket, 1, CHARP, key);

mSCRIPT_DEFINE_STRUCT(mScriptStorageContext)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptStorageContext)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptStorageContext, getBucket)
mSCRIPT_DEFINE_END;

struct mScriptValue* mScriptStorageBucketGet(struct mScriptStorageBucket* bucket, const char* key) {
	struct mScriptValue* val = mScriptTableLookup(bucket->root, &mSCRIPT_MAKE_CHARP(key));
	if (val) {
		mScriptValueRef(val);
	}
	return val;
}

void mScriptStorageBucketSet(struct mScriptStorageBucket* bucket, const char* key, struct mScriptValue* value) {
	struct mScriptValue* vkey = mScriptStringCreateFromUTF8(key);
	if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
		value = mScriptValueUnwrap(value);
	}
	mScriptTableInsert(bucket->root, vkey, value);
	mScriptValueDeref(vkey);
	bucket->dirty = true;
}

void mScriptStorageBucketSetVoid(struct mScriptStorageBucket* bucket, const char* key, struct mScriptValue* value) {
	UNUSED(value);
	struct mScriptValue* vkey = mScriptStringCreateFromUTF8(key);
	mScriptTableInsert(bucket->root, vkey, &mScriptValueNull);
	mScriptValueDeref(vkey);
	bucket->dirty = true;
}

#define MAKE_SCALAR_SETTER(NAME, TYPE) \
	void mScriptStorageBucketSet ## NAME (struct mScriptStorageBucket* bucket, const char* key, mSCRIPT_TYPE_C_ ## TYPE value) { \
		struct mScriptValue* vkey = mScriptStringCreateFromUTF8(key); \
		struct mScriptValue* vval = mScriptValueAlloc(mSCRIPT_TYPE_MS_ ## TYPE); \
		vval->value.mSCRIPT_TYPE_FIELD_ ## TYPE = value; \
		mScriptTableInsert(bucket->root, vkey, vval); \
		mScriptValueDeref(vkey); \
		mScriptValueDeref(vval); \
		bucket->dirty = true; \
	}

MAKE_SCALAR_SETTER(SInt, S64)
MAKE_SCALAR_SETTER(UInt, U64)
MAKE_SCALAR_SETTER(Float, F64)
MAKE_SCALAR_SETTER(Bool, BOOL)

void mScriptContextAttachStorage(struct mScriptContext* context) {
	struct mScriptStorageContext* storage = calloc(1, sizeof(*storage));
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptStorageContext));
	value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	value->value.opaque = storage;

	HashTableInit(&storage->buckets, 0, mScriptStorageBucketDeinit);

	mScriptContextSetGlobal(context, "storage", value);
	mScriptContextSetDocstring(context, "storage", "Singleton instance of struct::mScriptStorageContext");
}

void mScriptStorageContextDeinit(struct mScriptStorageContext* storage) {
	HashTableDeinit(&storage->buckets);
}

struct mScriptStorageBucket* mScriptStorageGetBucket(struct mScriptStorageContext* storage, const char* name) {
	if (!name) {
		return NULL;
	}

	// Check if name is allowed
	// Currently only names matching /[0-9A-Za-z_.]+/ are allowed
	size_t i;
	for (i = 0; name[i]; ++i) {
		if (i >= STORAGE_LEN_MAX) {
			return NULL;
		}
		if (!isalnum(name[i]) && name[i] != '_' && name[i] != '.') {
			return NULL;
		}
	}
	struct mScriptStorageBucket* bucket = HashTableLookup(&storage->buckets, name);
	if (bucket) {
		return bucket;
	}

	bucket = calloc(1, sizeof(*bucket));
	bucket->root = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	bucket->name = strdup(name);
	HashTableInsert(&storage->buckets, name, bucket);
	return bucket;
}

void mScriptStorageBucketDeinit(void* data) {
	struct mScriptStorageBucket* bucket = data;
	mScriptValueDeref(bucket->root);
	free(bucket->name);
	free(bucket);
}
