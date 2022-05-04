/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/types.h>

#include <mgba-util/hash.h>
#include <mgba-util/table.h>

#define MAX_ALIGNMENT 8

static void _allocTable(struct mScriptValue*);
static void _freeTable(struct mScriptValue*);
static void _deinitTableValue(void*);

static void _allocString(struct mScriptValue*);
static void _freeString(struct mScriptValue*);
static uint32_t _hashString(const struct mScriptValue*);

static bool _castScalar(const struct mScriptValue*, const struct mScriptType*, struct mScriptValue*);
static uint32_t _hashScalar(const struct mScriptValue*);

static uint32_t _valHash(const void* val, size_t len, uint32_t seed);
static bool _valEqual(const void* a, const void* b);
static void* _valRef(void*);
static void _valDeref(void*);

static bool _typeEqual(const struct mScriptValue*, const struct mScriptValue*);
static bool _s32Equal(const struct mScriptValue*, const struct mScriptValue*);
static bool _u32Equal(const struct mScriptValue*, const struct mScriptValue*);
static bool _f32Equal(const struct mScriptValue*, const struct mScriptValue*);
static bool _s64Equal(const struct mScriptValue*, const struct mScriptValue*);
static bool _u64Equal(const struct mScriptValue*, const struct mScriptValue*);
static bool _f64Equal(const struct mScriptValue*, const struct mScriptValue*);
static bool _charpEqual(const struct mScriptValue*, const struct mScriptValue*);
static bool _stringEqual(const struct mScriptValue*, const struct mScriptValue*);

const struct mScriptType mSTVoid = {
	.base = mSCRIPT_TYPE_VOID,
	.size = 0,
	.name = "void",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
	.equal = _typeEqual,
	.cast = NULL,
};

const struct mScriptType mSTSInt8 = {
	.base = mSCRIPT_TYPE_SINT,
	.size = 1,
	.name = "s8",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _s32Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTUInt8 = {
	.base = mSCRIPT_TYPE_UINT,
	.size = 1,
	.name = "u8",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _u32Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTSInt16 = {
	.base = mSCRIPT_TYPE_SINT,
	.size = 2,
	.name = "s16",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _s32Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTUInt16 = {
	.base = mSCRIPT_TYPE_UINT,
	.size = 2,
	.name = "u16",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _u32Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTSInt32 = {
	.base = mSCRIPT_TYPE_SINT,
	.size = 4,
	.name = "s32",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _s32Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTUInt32 = {
	.base = mSCRIPT_TYPE_UINT,
	.size = 4,
	.name = "u32",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _u32Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTFloat32 = {
	.base = mSCRIPT_TYPE_FLOAT,
	.size = 4,
	.name = "f32",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
	.equal = _f32Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTSInt64 = {
	.base = mSCRIPT_TYPE_SINT,
	.size = 8,
	.name = "s64",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _s64Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTUInt64 = {
	.base = mSCRIPT_TYPE_UINT,
	.size = 8,
	.name = "u64",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _u64Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTFloat64 = {
	.base = mSCRIPT_TYPE_FLOAT,
	.size = 8,
	.name = "f64",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
	.equal = _f64Equal,
	.cast = _castScalar,
};

const struct mScriptType mSTString = {
	.base = mSCRIPT_TYPE_STRING,
	.size = sizeof(struct mScriptString),
	.name = "string",
	.alloc = _allocString,
	.free = _freeString,
	.hash = _hashString,
	.equal = _stringEqual,
};

const struct mScriptType mSTCharPtr = {
	.base = mSCRIPT_TYPE_STRING,
	.size = sizeof(char*),
	.name = "charptr",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashString,
	.equal = _charpEqual,
};

const struct mScriptType mSTTable = {
	.base = mSCRIPT_TYPE_TABLE,
	.size = sizeof(struct Table),
	.name = "table",
	.alloc = _allocTable,
	.free = _freeTable,
	.hash = NULL,
};

const struct mScriptType mSTWrapper = {
	.base = mSCRIPT_TYPE_WRAPPER,
	.size = sizeof(struct mScriptValue),
	.name = "wrapper",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
};

DEFINE_VECTOR(mScriptList, struct mScriptValue)

void _allocTable(struct mScriptValue* val) {
	val->value.opaque = malloc(sizeof(struct Table));
	struct TableFunctions funcs = {
		.deinitializer = _deinitTableValue,
		.hash = _valHash,
		.equal = _valEqual,
		.ref = _valRef,
		.deref = _valDeref
	};
	HashTableInitCustom(val->value.opaque, 0, &funcs);
}

void _freeTable(struct mScriptValue* val) {
	HashTableDeinit(val->value.opaque);
	free(val->value.opaque);
}

void _deinitTableValue(void* val) {
	mScriptValueDeref(val);
}

static void _allocString(struct mScriptValue* val) {
	struct mScriptString* string = calloc(1, sizeof(*string));
	string->size = 0;
	string->buffer = NULL;
	val->value.opaque = string;
}

static void _freeString(struct mScriptValue* val) {
	struct mScriptString* string = val->value.opaque;
	if (string->size) {
		free(string->buffer);
	}
	free(string);
}

static uint32_t _hashString(const struct mScriptValue* val) {
	const char* buffer = 0;
	size_t size = 0;
	if (val->type == &mSTString) {
		struct mScriptString* string = val->value.opaque;
		buffer = string->buffer;
		size = string->size;
	} else if (val->type == &mSTCharPtr) {
		buffer = val->value.opaque;
		size = strlen(buffer);
	}
	return hash32(buffer, size, 0);
}

uint32_t _hashScalar(const struct mScriptValue* val) {
	// From https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
	uint32_t x = 0;
	switch (val->type->base) {
	case mSCRIPT_TYPE_SINT:
		x = val->value.s32;
		break;
	case mSCRIPT_TYPE_UINT:
	default:
		x = val->value.u32;
		break;
	}
	x = ((x >> 16) ^ x) * 0x45D9F3B;
	x = ((x >> 16) ^ x) * 0x45D9F3B;
	x = (x >> 16) ^ x;
	return x;
}

#define AS(NAME, TYPE) \
	bool _as ## NAME(const struct mScriptValue* input, mSCRIPT_TYPE_C_ ## TYPE * T) { \
		switch (input->type->base) { \
		case mSCRIPT_TYPE_SINT: \
			if (input->type->size <= 4) { \
				*T = input->value.s32; \
			} else if (input->type->size == 8) { \
				*T = input->value.s64; \
			} \
			break; \
		case mSCRIPT_TYPE_UINT: \
			if (input->type->size <= 4) { \
				*T = input->value.u32; \
			} else if (input->type->size == 8) { \
				*T = input->value.u64; \
			} \
			break; \
		case mSCRIPT_TYPE_FLOAT: \
			if (input->type->size == 4) { \
				*T = input->value.f32; \
			} else if (input->type->size == 8) { \
				*T = input->value.f64; \
			} \
			break; \
		default: \
			return false; \
		} \
		return true; \
	}

_mAPPLY(AS(SInt32, S32));
_mAPPLY(AS(UInt32, U32));
_mAPPLY(AS(Float32, F32));
_mAPPLY(AS(SInt64, S64));
_mAPPLY(AS(UInt64, U64));
_mAPPLY(AS(Float64, F64));

bool _castScalar(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) {
	switch (type->base) {
	case mSCRIPT_TYPE_SINT:
		if (type->size <= 4) {
			if (!_asSInt32(input, &output->value.s32)) {
				return false;
			}
		} else if (type->size == 8) {
			if (!_asSInt64(input, &output->value.s64)) {
				return false;
			}
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		if (type->size <= 4) {
			if (!_asUInt32(input, &output->value.u32)) {
				return false;
			}
		} else if (type->size == 8) {
			if (!_asUInt64(input, &output->value.u64)) {
				return false;
			}
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_FLOAT:
		if (type->size == 4) {
			if (!_asFloat32(input, &output->value.f32)) {
				return false;
			}
		} else if (type->size == 8) {
			if (!_asFloat64(input, &output->value.f64)) {
				return false;
			}
		} else {
			return false;
		}
		break;
	default:
		return false;
	}
	output->type = type;
	return true;
}

uint32_t _valHash(const void* val, size_t len, uint32_t seed) {
	UNUSED(len);
	const struct mScriptValue* value = val;
	uint32_t hash = value->type->hash(value);
	return hash ^ seed;
}

bool _valEqual(const void* a, const void* b) {
	const struct mScriptValue* valueA = a;
	const struct mScriptValue* valueB = b;
	return valueA->type->equal(valueA, valueB);
}

void* _valRef(void* val) {
	mScriptValueRef(val);
	return val;
}

void _valDeref(void* val) {
	mScriptValueDeref(val);
}

bool _typeEqual(const struct mScriptValue* a, const struct mScriptValue* b) {
	return a->type == b->type;
}

bool _s32Equal(const struct mScriptValue* a, const struct mScriptValue* b) {
	int32_t val;
	switch (b->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (b->type->size <= 4) {
			val = b->value.s32;
		} else if (b->type->size == 8) {
			if (b->value.s64 > INT_MAX || b->value.s64 < INT_MIN) {
				return false;
			}
			val = b->value.s64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		if (a->value.s32 < 0) {
			return false;
		}
		if (b->type->size <= 4) {
			if (b->value.u32 > (uint32_t) INT_MAX) {
				return false;
			}
			val = b->value.u32;
		} else if (b->type->size == 8) {
			if (b->value.u64 > (uint64_t) INT_MAX) {
				return false;
			}
			val = b->value.u64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
	return a->value.s32 == val;
}

bool _u32Equal(const struct mScriptValue* a, const struct mScriptValue* b) {
	uint32_t val;
	switch (b->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (b->type->size <= 4) {
			if (a->value.u32 > (uint32_t) INT_MAX) {
				return false;
			}
			if (b->value.s32 < 0) {
				return false;
			}
			val = b->value.s32;
		} else if (b->type->size == 8) {
			if (b->value.s64 > (int64_t) UINT_MAX || b->value.s64 < 0) {
				return false;
			}
			val = b->value.s64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		if (b->type->size <= 4) {
			val = b->value.u32;
		} else if (b->type->size == 8) {
			if (b->value.u64 > UINT_MAX) {
				return false;
			}
			val = b->value.u64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
	return a->value.u32 == val;
}

bool _f32Equal(const struct mScriptValue* a, const struct mScriptValue* b) {
	float val;
	switch (b->type->base) {
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_FLOAT:
		if (!_asFloat32(b, &val)) {
			return false;
		}
		break;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
	return a->value.f32 == val;
}

bool _s64Equal(const struct mScriptValue* a, const struct mScriptValue* b) {
	int64_t val;
	switch (b->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (b->type->size <= 4) {
			val = b->value.s32;
		} else if (b->type->size == 8) {
			val = b->value.s64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		if (a->value.s64 < 0) {
			return false;
		}
		if (b->type->size <= 4) {
			val = b->value.u32;
		} else if (b->type->size == 8) {
			if (b->value.u64 > (uint64_t) INT64_MAX) {
				return false;
			}
			val = b->value.u64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
	return a->value.s64 == val;
}

bool _u64Equal(const struct mScriptValue* a, const struct mScriptValue* b) {
	uint64_t val;
	switch (b->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (b->type->size <= 4) {
			if (a->value.u64 > (uint64_t) INT_MAX) {
				return false;
			}
			if (b->value.s32 < 0) {
				return false;
			}
			val = b->value.s32;
		} else if (b->type->size == 8) {
			if (a->value.u64 > (uint64_t) INT64_MAX) {
				return false;
			}
			if (b->value.s64 < 0) {
				return false;
			}
			val = b->value.s64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		if (b->type->size <= 4) {
			val = b->value.u32;
		} else if (b->type->size == 8) {
			val = b->value.u64;
		} else {
			return false;
		}
		break;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
	return a->value.u64 == val;
}

bool _f64Equal(const struct mScriptValue* a, const struct mScriptValue* b) {
	double val;
	switch (b->type->base) {
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_FLOAT:
		if (!_asFloat64(b, &val)) {
			return false;
		}
		break;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
	return a->value.f64 == val;
}

bool _charpEqual(const struct mScriptValue* a, const struct mScriptValue* b) {
	const char* valA = a->value.opaque;
	const char* valB;
	size_t lenA;
	size_t lenB;
	if (b->type->base != mSCRIPT_TYPE_STRING) {
		return false;
	}
	if (b->type == &mSTCharPtr) {
		valB = b->value.opaque;
		lenB = strlen(valB);
	} else if (b->type == &mSTString) {
		struct mScriptString* stringB = b->value.opaque;
		valB = stringB->buffer;
		lenB = stringB->size;
	} else {
		// TODO: Allow casting
		return false;
	}
	lenA = strlen(valA);
	if (lenA != lenB) {
		return false;
	}
	return strncmp(valA, valB, lenA) == 0;
}

bool _stringEqual(const struct mScriptValue* a, const struct mScriptValue* b) {
	struct mScriptString* stringA = a->value.opaque;
	const char* valA = stringA->buffer;
	const char* valB;
	size_t lenA = stringA->size;
	size_t lenB;
	if (b->type->base != mSCRIPT_TYPE_STRING) {
		return false;
	}
	if (b->type == &mSTCharPtr) {
		valB = b->value.opaque;
		lenB = strlen(valB);
	} else if (b->type == &mSTString) {
		struct mScriptString* stringB = b->value.opaque;
		valB = stringB->buffer;
		lenB = stringB->size;
	} else {
		// TODO: Allow casting
		return false;
	}
	if (lenA != lenB) {
		return false;
	}
	return strncmp(valA, valB, lenA) == 0;
}

struct mScriptValue* mScriptValueAlloc(const struct mScriptType* type) {
	// TODO: Use an arena instead of just the generic heap
	struct mScriptValue* val = malloc(sizeof(*val));
	val->refs = 1;
	val->type = type;
	if (type->alloc) {
		type->alloc(val);
	} else {
		val->value.opaque = NULL;
	}
	return val;
}

void mScriptValueRef(struct mScriptValue* val) {
	if (val->refs == INT_MAX) {
		abort();
	} else if (val->refs == mSCRIPT_VALUE_UNREF) {
		return;
	}
	++val->refs;
}

void mScriptValueDeref(struct mScriptValue* val) {
	if (val->refs > 1) {
		--val->refs;
		return;
	} else if (val->refs <= 0) {
		return;
	}
	if (val->type->free) {
		val->type->free(val);
	}
	free(val);
}

void mScriptValueWrap(struct mScriptValue* value, struct mScriptValue* out) {
	if (value->refs == mSCRIPT_VALUE_UNREF) {
		memcpy(out, value, sizeof(*out));
		return;
	}
	out->refs = mSCRIPT_VALUE_UNREF;
	switch (value->type->base) {
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_FLOAT:
	case mSCRIPT_TYPE_WRAPPER:
		out->type = value->type;
		memcpy(&out->value, &value->value, sizeof(out->value));
		return;
	default:
		break;
	}

	out->type = mSCRIPT_TYPE_MS_WRAPPER;
	out->value.opaque = value;
	mScriptValueRef(value);
}

struct mScriptValue* mScriptValueUnwrap(struct mScriptValue* value) {
	if (value->type == mSCRIPT_TYPE_MS_WRAPPER) {
		return value->value.opaque;
	}
	return NULL;
}

const struct mScriptValue* mScriptValueUnwrapConst(const struct mScriptValue* value) {
	if (value->type == mSCRIPT_TYPE_MS_WRAPPER) {
		return value->value.copaque;
	}
	return NULL;
}

struct mScriptValue* mScriptStringCreateFromUTF8(const char* string) {
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_STR);
	struct mScriptString* internal = val->value.opaque;
	internal->size = strlen(string);
	internal->buffer = strdup(string);
	return val;
}

bool mScriptTableInsert(struct mScriptValue* table, struct mScriptValue* key, struct mScriptValue* value) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	if (!key->type->hash) {
		return false;
	}
	struct Table* t = table->value.opaque;
	mScriptValueRef(value);
	HashTableInsertCustom(t, key, value);
	return true;
}

bool mScriptTableRemove(struct mScriptValue* table, struct mScriptValue* key) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	if (!key->type->hash) {
		return false;
	}
	struct Table* t = table->value.opaque;
	HashTableRemoveCustom(t, key);
	return true;
}

struct mScriptValue* mScriptTableLookup(struct mScriptValue* table, struct mScriptValue* key) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	if (!key->type->hash) {
		return false;
	}
	struct Table* t = table->value.opaque;
	return HashTableLookupCustom(t, key);
}

void mScriptFrameInit(struct mScriptFrame* frame) {
	mScriptListInit(&frame->arguments, 4);
	mScriptListInit(&frame->returnValues, 1);
}

void mScriptFrameDeinit(struct mScriptFrame* frame) {
	mScriptListDeinit(&frame->returnValues);
	mScriptListDeinit(&frame->arguments);
}

static void _mScriptClassInit(struct mScriptTypeClass* cls, const struct mScriptClassInitDetails* details, bool child) {
	const char* docstring = NULL;
	size_t staticOffset = 0;

	size_t i;
	for (i = 0; details[i].type != mSCRIPT_CLASS_INIT_END; ++i) {
		const struct mScriptClassInitDetails* detail = &details[i];
		struct mScriptClassMember* member;

		switch (detail->type) {
		case mSCRIPT_CLASS_INIT_END:
			break;
		case mSCRIPT_CLASS_INIT_DOCSTRING:
			docstring = detail->info.comment;
			break;
		case mSCRIPT_CLASS_INIT_INHERIT:
			member = calloc(1, sizeof(*member));
			member->name = "_super";
			member->type = detail->info.parent;
			if (!child) {
				cls->parent = detail->info.parent;
			}
			HashTableInsert(&cls->instanceMembers, member->name, member);
			_mScriptClassInit(cls, detail->info.parent->details.cls->details, true);
			break;
		case mSCRIPT_CLASS_INIT_INSTANCE_MEMBER:
			member = calloc(1, sizeof(*member));
			memcpy(member, &detail->info.member, sizeof(*member));
			if (docstring) {
				member->docstring = docstring;
				docstring = NULL;
			}
			HashTableInsert(&cls->instanceMembers, member->name, member);
			break;
		case mSCRIPT_CLASS_INIT_STATIC_MEMBER:
			if (!child) {
				member = calloc(1, sizeof(*member));
				memcpy(member, &detail->info.member, sizeof(*member));
				if (docstring) {
					member->docstring = docstring;
					docstring = NULL;
				}

				// Alignment check
				if (staticOffset & (detail->info.member.type->size - 1)) {
					size_t size = detail->info.member.type->size;
					if (size > MAX_ALIGNMENT) {
						size = MAX_ALIGNMENT;
					}
					staticOffset = (staticOffset & ~(size - 1)) + size;
				}
				member->offset = staticOffset;
				staticOffset += detail->info.member.type->size;
				HashTableInsert(&cls->staticMembers, member->name, member);
			}
			break;
		}
	}
}

void mScriptClassInit(struct mScriptTypeClass* cls) {
	if (cls->init) {
		return;
	}
	HashTableInit(&cls->staticMembers, 0, free);
	HashTableInit(&cls->instanceMembers, 0, free);

	_mScriptClassInit(cls, cls->details, false);

	cls->init = true;
}

void mScriptClassDeinit(struct mScriptTypeClass* cls) {
	if (!cls->init) {
		return;
	}
	HashTableDeinit(&cls->instanceMembers);
	HashTableDeinit(&cls->staticMembers);
	cls->init = false;
}

bool mScriptObjectGet(struct mScriptValue* obj, const char* member, struct mScriptValue* val) {
	if (obj->type->base == mSCRIPT_TYPE_WRAPPER) {
		obj = mScriptValueUnwrap(obj);
	}
	if (obj->type->base != mSCRIPT_TYPE_OBJECT) {
		return false;
	}

	struct mScriptTypeClass* cls = obj->type->details.cls;
	if (!cls) {
		return false;
	}

	mScriptClassInit(cls);

	struct mScriptClassMember* m = HashTableLookup(&cls->instanceMembers, member);
	if (!m) {
		return false;
	}

	void* rawMember = (void *)((uintptr_t) obj->value.opaque + m->offset);
	switch (m->type->base) {
	case mSCRIPT_TYPE_SINT:
		switch (m->type->size) {
		case 1:
			*val = mSCRIPT_MAKE_S32(*(int8_t *) rawMember);
			break;
		case 2:
			*val = mSCRIPT_MAKE_S32(*(int16_t *) rawMember);
			break;
		case 4:
			*val = mSCRIPT_MAKE_S32(*(int32_t *) rawMember);
			break;
		case 8:
			*val = mSCRIPT_MAKE_S64(*(int64_t *) rawMember);
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		switch (m->type->size) {
		case 1:
			*val = mSCRIPT_MAKE_U32(*(uint8_t *) rawMember);
			break;
		case 2:
			*val = mSCRIPT_MAKE_U32(*(uint16_t *) rawMember);
			break;
		case 4:
			*val = mSCRIPT_MAKE_U32(*(uint32_t *) rawMember);
			break;
		case 8:
			*val = mSCRIPT_MAKE_U64(*(uint64_t *) rawMember);
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_FLOAT:
		switch (m->type->size) {
		case 4:
			*val = mSCRIPT_MAKE_F32(*(mSCRIPT_TYPE_C_F32 *) rawMember);
			break;
		case 8:
			*val = mSCRIPT_MAKE_F64(*(mSCRIPT_TYPE_C_F64 *) rawMember);
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_FUNCTION:
		val->refs = mSCRIPT_VALUE_UNREF;
		val->type = m->type;
		m->type->alloc(val);
		break;
	case mSCRIPT_TYPE_OBJECT:
		val->refs = mSCRIPT_VALUE_UNREF;
		val->value.opaque = rawMember;
		if (obj->type->isConst && !m->type->isConst) {
			val->type = m->type->constType;
		} else {
			val->type = m->type;
		}
		break;
	default:
		return false;
	}
	return true;
}

bool mScriptObjectSet(struct mScriptValue* obj, const char* member, struct mScriptValue* val) {
	if (obj->type->base != mSCRIPT_TYPE_OBJECT || obj->type->isConst) {
		return false;
	}

	struct mScriptTypeClass* cls = obj->type->details.cls;
	if (!cls) {
		return false;
	}

	mScriptClassInit(cls);

	struct mScriptClassMember* m = HashTableLookup(&cls->instanceMembers, member);
	if (!m) {
		return false;
	}

	void* rawMember = (void *)((uintptr_t) obj->value.opaque + m->offset);
	if (m->type != val->type) {
		if (!mScriptCast(m->type, val, val)) {
			return false;
		}
	}

	switch (m->type->base) {
	case mSCRIPT_TYPE_SINT:
		switch (m->type->size) {
		case 1:
			*(int8_t *) rawMember = val->value.s32;
			break;
		case 2:
			*(int16_t *) rawMember = val->value.s32;
			break;
		case 4:
			*(int32_t *) rawMember = val->value.s32;
			break;
		case 8:
			*(int64_t *) rawMember = val->value.s64;
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		switch (m->type->size) {
		case 1:
			*(uint8_t *) rawMember = val->value.u32;
			break;
		case 2:
			*(uint16_t *) rawMember = val->value.u32;
			break;
		case 4:
			*(uint32_t *) rawMember = val->value.u32;
			break;
		case 8:
			*(uint64_t *) rawMember = val->value.u64;
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_FLOAT:
		switch (m->type->size) {
		case 4:
			*(mSCRIPT_TYPE_C_F32 *) rawMember = val->value.f32;
			break;
		case 8:
			*(mSCRIPT_TYPE_C_F64 *) rawMember = val->value.f64;
			break;
		default:
			return false;
		}
		break;
	default:
		return false;
	}
	return true;
}

bool mScriptPopS32(struct mScriptList* list, int32_t* out) {
	mSCRIPT_POP(list, S32, val);
	*out = val;
	return true;
}

bool mScriptPopS64(struct mScriptList* list, int64_t* out) {
	mSCRIPT_POP(list, S64, val);
	*out = val;
	return true;
}

bool mScriptPopU32(struct mScriptList* list, uint32_t* out) {
	mSCRIPT_POP(list, U32, val);
	*out = val;
	return true;
}

bool mScriptPopU64(struct mScriptList* list, uint64_t* out) {
	mSCRIPT_POP(list, U64, val);
	*out = val;
	return true;
}

bool mScriptPopF32(struct mScriptList* list, float* out) {
	mSCRIPT_POP(list, F32, val);
	*out = val;
	return true;
}

bool mScriptPopF64(struct mScriptList* list, double* out) {
	mSCRIPT_POP(list, F64, val);
	*out = val;
	return true;
}

bool mScriptPopPointer(struct mScriptList* list, void** out) {
	mSCRIPT_POP(list, PTR, val);
	*out = val;
	return true;
}

bool mScriptCast(const struct mScriptType* type, const struct mScriptValue* input, struct mScriptValue* output) {
	if (input->type->base == mSCRIPT_TYPE_WRAPPER) {
		input = mScriptValueUnwrapConst(input);
	}
	if (type->cast && type->cast(input, type, output)) {
		return true;
	}
	if (input->type->cast && input->type->cast(input, type, output)) {
		return true;
	}
	return false;
}

bool mScriptCoerceFrame(const struct mScriptTypeTuple* types, struct mScriptList* frame) {
	if (types->count != mScriptListSize(frame) && (!types->variable || mScriptListSize(frame) < types->count)) {
		return false;
	}
	size_t i;
	for (i = 0; i < types->count; ++i) {
		if (types->entries[i] == mScriptListGetPointer(frame, i)->type) {
			continue;
		}
		if (!mScriptCast(types->entries[i], mScriptListGetPointer(frame, i), mScriptListGetPointer(frame, i))) {
			return false;
		}
	}
	return true;
}
