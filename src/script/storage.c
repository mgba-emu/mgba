/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/storage.h>

#include <mgba/core/config.h>
#include <mgba-util/vfs.h>

#include <json.h>
#include <sys/stat.h>

#define STORAGE_LEN_MAX 64

struct mScriptStorageBucket {
	char* name;
	struct mScriptValue* root;
	bool autoflush;
	bool dirty;
};

struct mScriptStorageContext {
	struct Table buckets;
};

void mScriptStorageBucketDeinit(void*);
struct mScriptValue* mScriptStorageBucketGet(struct mScriptStorageBucket* bucket, const char* key);
static void mScriptStorageBucketSet(struct mScriptStorageBucket* bucket, const char* key, struct mScriptValue* value);
static void mScriptStorageBucketSetVoid(struct mScriptStorageBucket* bucket, const char* key, struct mScriptValue* value);
static void mScriptStorageBucketSetSInt(struct mScriptStorageBucket* bucket, const char* key, int64_t value);
static void mScriptStorageBucketSetUInt(struct mScriptStorageBucket* bucket, const char* key, uint64_t value);
static void mScriptStorageBucketSetFloat(struct mScriptStorageBucket* bucket, const char* key, double value);
static void mScriptStorageBucketSetBool(struct mScriptStorageBucket* bucket, const char* key, bool value);
static bool mScriptStorageBucketReload(struct mScriptStorageBucket* bucket);
static bool mScriptStorageBucketFlush(struct mScriptStorageBucket* bucket);
static void mScriptStorageBucketEnableAutoFlush(struct mScriptStorageBucket* bucket, bool enable);

static void mScriptStorageContextDeinit(struct mScriptStorageContext*);
static void mScriptStorageContextFlushAll(struct mScriptStorageContext*);
struct mScriptStorageBucket* mScriptStorageGetBucket(struct mScriptStorageContext*, const char* name);

static bool mScriptStorageToJson(struct mScriptValue* value, struct json_object** out);
static struct mScriptValue* mScriptStorageFromJson(struct json_object* json);

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
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptStorageBucket, BOOL, reload, mScriptStorageBucketReload, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptStorageBucket, BOOL, flush, mScriptStorageBucketFlush, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageBucket, enableAutoFlush, mScriptStorageBucketEnableAutoFlush, 1, BOOL, enable);

mSCRIPT_DEFINE_STRUCT(mScriptStorageBucket)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A single 'bucket' of stored data, appropriate for a single script to store its data. "
		"Fields can be set directly on the bucket objct, e.g. if you want to store a value called "
		"`foo` on a bucket named `bucket`, you can directly assign to it as `bucket.foo = value`, "
		"and retrieve it in the same way later. Primitive types (numbers, strings, lists and tables) "
		"can be stored in buckets, but complex data types (e.g. a bucket itself) cannot. Data "
		"stored in a bucket is periodically flushed to disk and persists between sessions."
	)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setSInt)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setUInt)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setFloat)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setBool)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setStr)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setList)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setTable)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(mScriptStorageBucket, setVoid)
	mSCRIPT_DEFINE_STRUCT_DEFAULT_GET(mScriptStorageBucket)
	mSCRIPT_DEFINE_DOCSTRING("Reload the state of the bucket from disk")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptStorageBucket, reload)
	mSCRIPT_DEFINE_DOCSTRING("Flush the bucket to disk manually")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptStorageBucket, flush)
	mSCRIPT_DEFINE_DOCSTRING(
		"Enable or disable the automatic flushing of this bucket. This is good for ensuring buckets "
		"don't get flushed in an inconsistent state. It will also disable flushing to disk when the "
		"emulator is shut down, so make sure to either manually flush the bucket or re-enable "
		"automatic flushing whenever you're done updating it if you do disable it prior."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptStorageBucket, enableAutoFlush)
mSCRIPT_DEFINE_END;

mSCRIPT_DECLARE_STRUCT(mScriptStorageContext);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageContext, _deinit, mScriptStorageContextDeinit, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(mScriptStorageContext, S(mScriptStorageBucket), getBucket, mScriptStorageGetBucket, 1, CHARP, key);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptStorageContext, flushAll, mScriptStorageContextFlushAll, 0);

mSCRIPT_DEFINE_STRUCT(mScriptStorageContext)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptStorageContext)
	mSCRIPT_DEFINE_DOCSTRING(
		"Get a bucket with the given name. Names can contain letters, numbers, "
		"underscores and periods. If a given bucket doesn't exist, it is created."
	)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptStorageContext, getBucket)
	mSCRIPT_DEFINE_DOCSTRING("Flush all buckets to disk manually")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptStorageContext, flushAll)
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

void mScriptStorageGetBucketPath(const char* bucket, char* out) {
	mCoreConfigDirectory(out, PATH_MAX);

	strncat(out, PATH_SEP "storage" PATH_SEP, PATH_MAX - 1);
#ifdef _WIN32
	// TODO: Move this to vfs somewhere
	WCHAR wout[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, out, -1, wout, MAX_PATH);
	CreateDirectoryW(wout, NULL);
#else
	mkdir(out, 0755);
#endif

	char suffix[STORAGE_LEN_MAX + 6];
	snprintf(suffix, sizeof(suffix), "%s.json", bucket);
	strncat(out, suffix, PATH_MAX - 1);
}

static struct json_object* _tableToJson(struct mScriptValue* rootVal) {
	bool ok = true;

	struct TableIterator iter;
	struct json_object* rootObj = json_object_new_object();
	if (mScriptTableIteratorStart(rootVal, &iter)) {
		do {
			struct mScriptValue* key = mScriptTableIteratorGetKey(rootVal, &iter);
			struct mScriptValue* value = mScriptTableIteratorGetValue(rootVal, &iter);
			const char* ckey;
			if (key->type == mSCRIPT_TYPE_MS_CHARP) {
				ckey = key->value.copaque;
			} else if (key->type == mSCRIPT_TYPE_MS_STR) {
				ckey = key->value.string->buffer;
			} else {
				ok = false;
				break;
			}

			struct json_object* obj;
			ok = mScriptStorageToJson(value, &obj);

			if (ok) {
#if JSON_C_VERSION_NUM >= (13 << 8)
				ok = json_object_object_add(rootObj, ckey, obj) >= 0;
#else
				json_object_object_add(rootObj, ckey, obj);
#endif
			}
		} while (mScriptTableIteratorNext(rootVal, &iter) && ok);
	}
	if (!ok) {
		json_object_put(rootObj);
		return NULL;
	}
	return rootObj;
}

bool mScriptStorageToJson(struct mScriptValue* value, struct json_object** out) {
	if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
		value = mScriptValueUnwrap(value);
	}

	size_t i;
	bool ok = true;
	struct json_object* obj = NULL;
	switch (value->type->base) {
	case mSCRIPT_TYPE_SINT:
		obj = json_object_new_int64(value->value.s64);
		break;
	case mSCRIPT_TYPE_UINT:
		if (value->type == mSCRIPT_TYPE_MS_BOOL) {
			obj = json_object_new_boolean(value->value.u32);
			break;
		}
#if JSON_C_VERSION_NUM >= (14 << 8)
		obj = json_object_new_uint64(value->value.u64);
#else
		if (value->value.u64 < (uint64_t) INT64_MAX) {
			obj = json_object_new_int64(value->value.u64);
		} else {
			obj = json_object_new_double(value->value.u64);
		}
#endif
		break;
	case mSCRIPT_TYPE_FLOAT:
		obj = json_object_new_double(value->value.f64);
		break;
	case mSCRIPT_TYPE_STRING:
		obj = json_object_new_string_len(value->value.string->buffer, value->value.string->size);
		break;
	case mSCRIPT_TYPE_LIST:
#if JSON_C_VERSION_NUM >= (15 << 8)
		obj = json_object_new_array_ext(mScriptListSize(value->value.list));
#else
		obj = json_object_new_array();
#endif
		for (i = 0; i < mScriptListSize(value->value.list); ++i) {
			struct json_object* listObj;
			ok = mScriptStorageToJson(mScriptListGetPointer(value->value.list, i), &listObj);
			if (!ok) {
				break;
			}
			json_object_array_add(obj, listObj);
		}
		break;
	case mSCRIPT_TYPE_TABLE:
		obj = _tableToJson(value);
		if (!obj) {
			ok = false;
		}
		break;
	case mSCRIPT_TYPE_VOID:
		obj = NULL;
		break;
	default:
		ok = false;
		break;
	}

	if (!ok) {
		if (obj) {
			json_object_put(obj);
		}
		*out = NULL;
	} else {
		*out = obj;
	}
	return ok;
}

#ifndef JSON_C_TO_STRING_PRETTY_TAB
#define JSON_C_TO_STRING_PRETTY_TAB 0
#endif

static bool _mScriptStorageBucketFlushVF(struct mScriptStorageBucket* bucket, struct VFile* vf) {
	struct json_object* rootObj;
	bool ok = mScriptStorageToJson(bucket->root, &rootObj);
	if (!ok) {
		vf->close(vf);
		return false;
	}

	const char* json = json_object_to_json_string_ext(rootObj, JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_PRETTY_TAB);
	if (!json) {
		json_object_put(rootObj);
		vf->close(vf);
		return false;
	}

	vf->write(vf, json, strlen(json));
	vf->close(vf);

	bucket->dirty = false;

	json_object_put(rootObj);
	return true;
}

bool mScriptStorageBucketFlush(struct mScriptStorageBucket* bucket) {
	char path[PATH_MAX];
	mScriptStorageGetBucketPath(bucket->name, path);
	struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		return false;
	}
	return _mScriptStorageBucketFlushVF(bucket, vf);
}

void mScriptStorageBucketEnableAutoFlush(struct mScriptStorageBucket* bucket, bool enable) {
	bucket->autoflush = enable;
}

bool mScriptStorageSaveBucketVF(struct mScriptContext* context, const char* bucketName, struct VFile* vf) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "storage");
	if (!value) {
		vf->close(vf);
		return false;
	}
	struct mScriptStorageContext* storage = value->value.opaque;
	struct mScriptStorageBucket* bucket = mScriptStorageGetBucket(storage, bucketName);
	return _mScriptStorageBucketFlushVF(bucket, vf);
}

bool mScriptStorageSaveBucket(struct mScriptContext* context, const char* bucketName) {
	char path[PATH_MAX];
	mScriptStorageGetBucketPath(bucketName, path);
	struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (!vf) {
		return false;
	}
	return mScriptStorageSaveBucketVF(context, bucketName, vf);
}

struct mScriptValue* mScriptStorageFromJson(struct json_object* json) {
	enum json_type type = json_object_get_type(json);
	struct mScriptValue* value = NULL;
	switch (type) {
	case json_type_null:
		return &mScriptValueNull;
	case json_type_int:
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S64);
		value->value.s64 = json_object_get_int64(json);
		break;
	case json_type_double:
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_F64);
		value->value.f64 = json_object_get_double(json);
		break;
	case json_type_boolean:
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_BOOL);
		value->value.u32 = json_object_get_boolean(json);
		break;
	case json_type_string:
		value = mScriptStringCreateFromBytes(json_object_get_string(json), json_object_get_string_len(json));
		break;
	case json_type_array:
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_LIST);
		{
			size_t i;
			for (i = 0; i < json_object_array_length(json); ++i) {
				struct mScriptValue* vval = mScriptStorageFromJson(json_object_array_get_idx(json, i));
				if (!vval) {
					mScriptValueDeref(value);
					value = NULL;
					break;
				}
				mScriptValueWrap(vval, mScriptListAppend(value->value.list));
				mScriptValueDeref(vval);
			}
		}
		break;
	case json_type_object:
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
		{
			json_object_object_foreach(json, jkey, jval) {
				struct mScriptValue* vval = mScriptStorageFromJson(jval);
				if (!vval) {
					mScriptValueDeref(value);
					value = NULL;
					break;
				}
				struct mScriptValue* vkey = mScriptStringCreateFromUTF8(jkey);
				mScriptTableInsert(value, vkey, vval);
				mScriptValueDeref(vkey);
				mScriptValueDeref(vval);
			}
		}
		break;
	}
	return value;
}

static struct mScriptValue* _mScriptStorageLoadJson(struct VFile* vf) {
	ssize_t size = vf->size(vf);
	if (size < 2) {
		vf->close(vf);
		return NULL;
	}
	char* json = calloc(1, size + 1);
	if (vf->read(vf, json, size) != size) {
		vf->close(vf);
		return NULL;
	}
	vf->close(vf);

	struct json_object* obj = json_tokener_parse(json);
	free(json);
	if (!obj) {
		return NULL;
	}

	struct mScriptValue* root = mScriptStorageFromJson(obj);
	json_object_put(obj);
	return root;
}

bool mScriptStorageBucketReload(struct mScriptStorageBucket* bucket) {
	char path[PATH_MAX];
	mScriptStorageGetBucketPath(bucket->name, path);
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		return false;
	}
	struct mScriptValue* root = _mScriptStorageLoadJson(vf);
	if (!root) {
		return false;
	}
	if (bucket->root) {
		mScriptValueDeref(bucket->root);
	}
	bucket->root = root;

	bucket->dirty = false;

	return true;
}

bool mScriptStorageLoadBucketVF(struct mScriptContext* context, const char* bucketName, struct VFile* vf) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "storage");
	if (!value) {
		vf->close(vf);
		return false;
	}
	struct mScriptStorageContext* storage = value->value.opaque;
	struct mScriptValue* root = _mScriptStorageLoadJson(vf);
	if (!root) {
		return false;
	}
	struct mScriptStorageBucket* bucket = mScriptStorageGetBucket(storage, bucketName);
	mScriptValueDeref(bucket->root);
	bucket->root = root;

	bucket->dirty = false;

	return true;
}

bool mScriptStorageLoadBucket(struct mScriptContext* context, const char* bucketName) {
	char path[PATH_MAX];
	mScriptStorageGetBucketPath(bucketName, path);
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		return false;
	}
	return mScriptStorageLoadBucketVF(context, bucketName, vf);
}

void mScriptContextAttachStorage(struct mScriptContext* context) {
	struct mScriptStorageContext* storage = calloc(1, sizeof(*storage));
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptStorageContext));
	value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	value->value.opaque = storage;

	HashTableInit(&storage->buckets, 0, mScriptStorageBucketDeinit);

	mScriptContextSetGlobal(context, "storage", value);
	mScriptContextSetDocstring(context, "storage", "Singleton instance of struct::mScriptStorageContext");
}

void mScriptStorageFlushAll(struct mScriptContext* context) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "storage");
	if (!value) {
		return;
	}
	struct mScriptStorageContext* storage = value->value.opaque;
	mScriptStorageContextFlushAll(storage);
}

void mScriptStorageContextDeinit(struct mScriptStorageContext* storage) {
	HashTableDeinit(&storage->buckets);
}

void mScriptStorageContextFlushAll(struct mScriptStorageContext* storage) {
	struct TableIterator iter;
	if (HashTableIteratorStart(&storage->buckets, &iter)) {
		do {
			struct mScriptStorageBucket* bucket = HashTableIteratorGetValue(&storage->buckets, &iter);
			if (bucket->autoflush) {
				mScriptStorageBucketFlush(bucket);
			}
		} while (HashTableIteratorNext(&storage->buckets, &iter));
	}
}

struct mScriptStorageBucket* mScriptStorageGetBucket(struct mScriptStorageContext* storage, const char* name) {
	if (!name) {
		return NULL;
	}

	// Check if name is allowed
	// Currently only names matching /[0-9A-Za-z][0-9A-Za-z_.]*/ are allowed
	size_t i;
	for (i = 0; name[i]; ++i) {
		if (i >= STORAGE_LEN_MAX) {
			return NULL;
		}
		if (isalnum(name[i])) {
			continue;
		}
		if (name[i] == '_') {
			continue;
		}
		if (i > 0 && name[i] == '.') {
			continue;
		}
		return NULL;
	}
	struct mScriptStorageBucket* bucket = HashTableLookup(&storage->buckets, name);
	if (bucket) {
		return bucket;
	}

	bucket = calloc(1, sizeof(*bucket));
	bucket->name = strdup(name);
	bucket->autoflush = true;
	if (!mScriptStorageBucketReload(bucket)) {
		bucket->root = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	}
	HashTableInsert(&storage->buckets, name, bucket);
	return bucket;
}

void mScriptStorageBucketDeinit(void* data) {
	struct mScriptStorageBucket* bucket = data;
	if (bucket->dirty) {
		mScriptStorageBucketFlush(bucket);
	}
	mScriptValueDeref(bucket->root);
	free(bucket->name);
	free(bucket);
}
