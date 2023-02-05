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

void mScriptStorageGetBucketPath(const char* bucket, char* out) {
	mCoreConfigDirectory(out, PATH_MAX);

	strncat(out, PATH_SEP "storage" PATH_SEP, PATH_MAX);
	mkdir(out, 0755);

	char suffix[STORAGE_LEN_MAX + 6];
	snprintf(suffix, sizeof(suffix), "%s.json", bucket);
	strncat(out, suffix, PATH_MAX);
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

			if (!ok || json_object_object_add(rootObj, ckey, obj) < 0) {
				ok = false;
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
		obj = json_object_new_uint64(value->value.u64);
		break;
	case mSCRIPT_TYPE_FLOAT:
		obj = json_object_new_double(value->value.f64);
		break;
	case mSCRIPT_TYPE_STRING:
		obj = json_object_new_string_len(value->value.string->buffer, value->value.string->size);
		break;
	case mSCRIPT_TYPE_LIST:
		obj = json_object_new_array_ext(mScriptListSize(value->value.list));
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

bool mScriptStorageSaveBucketVF(struct mScriptContext* context, const char* bucketName, struct VFile* vf) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "storage");
	if (!value) {
		vf->close(vf);
		return false;
	}
	struct mScriptStorageContext* storage = value->value.opaque;
	struct mScriptStorageBucket* bucket = mScriptStorageGetBucket(storage, bucketName);
	struct json_object* rootObj;
	bool ok = mScriptStorageToJson(bucket->root, &rootObj);
	if (!ok) {
		vf->close(vf);
		return false;
	}

	const char* json = json_object_to_json_string_ext(rootObj, JSON_C_TO_STRING_PRETTY_TAB);
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

bool mScriptStorageSaveBucket(struct mScriptContext* context, const char* bucketName) {
	char path[PATH_MAX];
	mScriptStorageGetBucketPath(bucketName, path);
	struct VFile* vf = VFileOpen(path, O_WRONLY | O_CREAT | O_TRUNC);
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

bool mScriptStorageLoadBucketVF(struct mScriptContext* context, const char* bucketName, struct VFile* vf) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "storage");
	if (!value) {
		vf->close(vf);
		return false;
	}
	struct mScriptStorageContext* storage = value->value.opaque;

	ssize_t size = vf->size(vf);
	if (size < 2) {
		vf->close(vf);
		return false;
	}
	char* json = calloc(1, size + 1);
	if (vf->read(vf, json, size) != size) {
		vf->close(vf);
		return false;
	}
	vf->close(vf);

	struct json_object* obj = json_tokener_parse(json);
	free(json);
	if (!obj) {
		return false;
	}

	struct mScriptValue* root = mScriptStorageFromJson(obj);
	json_object_put(obj);
	if (!root) {
		return false;
	}

	struct mScriptStorageBucket* bucket = mScriptStorageGetBucket(storage, bucketName);
	mScriptValueDeref(bucket->root);
	bucket->root = root;

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
