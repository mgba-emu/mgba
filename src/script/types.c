/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/types.h>

#include <mgba/internal/script/types.h>
#include <mgba/script/context.h>
#include <mgba/script/macros.h>
#include <mgba-util/hash.h>
#include <mgba-util/string.h>
#include <mgba-util/table.h>

static void _allocList(struct mScriptValue*);
static void _freeList(struct mScriptValue*);

static void _allocTable(struct mScriptValue*);
static void _freeTable(struct mScriptValue*);
static void _deinitTableValue(void*);

static void _allocString(struct mScriptValue*);
static void _freeString(struct mScriptValue*);
static uint32_t _hashString(const struct mScriptValue*);
static bool _stringCast(const struct mScriptValue*, const struct mScriptType*, struct mScriptValue*);

static bool _castScalar(const struct mScriptValue*, const struct mScriptType*, struct mScriptValue*);
static uint32_t _hashScalar(const struct mScriptValue*);

static bool _wstrCast(const struct mScriptValue*, const struct mScriptType*, struct mScriptValue*);
static bool _wlistCast(const struct mScriptValue*, const struct mScriptType*, struct mScriptValue*);
static bool _wtableCast(const struct mScriptValue*, const struct mScriptType*, struct mScriptValue*);

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
static bool _boolEqual(const struct mScriptValue*, const struct mScriptValue*);
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

const struct mScriptType mSTBool = {
	.base = mSCRIPT_TYPE_UINT,
	.size = 1,
	.name = "bool",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
	.equal = _boolEqual,
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
	.cast = _stringCast,
};

const struct mScriptType mSTCharPtr = {
	.base = mSCRIPT_TYPE_STRING,
	.size = sizeof(char*),
	.name = "charptr",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashString,
	.equal = _charpEqual,
	.cast = _stringCast,
	.isConst = true,
};

const struct mScriptType mSTList = {
	.base = mSCRIPT_TYPE_LIST,
	.size = sizeof(struct mScriptList),
	.name = "list",
	.alloc = _allocList,
	.free = _freeList,
	.hash = NULL,
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

const struct mScriptType mSTStringWrapper = {
	.base = mSCRIPT_TYPE_WRAPPER,
	.size = sizeof(struct mScriptValue),
	.name = "wrapper string",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
	.cast = _wstrCast,
};

const struct mScriptType mSTListWrapper = {
	.base = mSCRIPT_TYPE_WRAPPER,
	.size = sizeof(struct mScriptValue),
	.name = "wrapper list",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
	.cast = _wlistCast,
};

const struct mScriptType mSTTableWrapper = {
	.base = mSCRIPT_TYPE_WRAPPER,
	.size = sizeof(struct mScriptValue),
	.name = "wrapper table",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
	.cast = _wtableCast,
};

const struct mScriptType mSTWeakref = {
	.base = mSCRIPT_TYPE_WEAKREF,
	.size = sizeof(uint32_t),
	.name = "weakref",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
};

struct mScriptValue mScriptValueNull = {
	.type = &mSTVoid,
	.refs = mSCRIPT_VALUE_UNREF
};

DEFINE_VECTOR(mScriptList, struct mScriptValue)

void _allocList(struct mScriptValue* val) {
	val->value.list = malloc(sizeof(struct mScriptList));
	mScriptListInit(val->value.list, 0);
}

void _freeList(struct mScriptValue* val) {
	size_t i;
	for (i = 0; i < mScriptListSize(val->value.list); ++i) {
		struct mScriptValue* value = mScriptListGetPointer(val->value.list, i);
		if (!value->type) {
			continue;
		}
		struct mScriptValue* unwrapped = mScriptValueUnwrap(value);
		if (unwrapped) {
			mScriptValueDeref(unwrapped);
		}
	}
	mScriptListDeinit(val->value.list);
	free(val->value.list);
}

void _allocTable(struct mScriptValue* val) {
	val->value.table = malloc(sizeof(struct Table));
	struct TableFunctions funcs = {
		.deinitializer = _deinitTableValue,
		.hash = _valHash,
		.equal = _valEqual,
		.ref = _valRef,
		.deref = _valDeref
	};
	HashTableInitCustom(val->value.table, 0, &funcs);
}

void _freeTable(struct mScriptValue* val) {
	HashTableDeinit(val->value.table);
	free(val->value.table);
}

void _deinitTableValue(void* val) {
	mScriptValueDeref(val);
}

static void _allocString(struct mScriptValue* val) {
	struct mScriptString* string = calloc(1, sizeof(*string));
	string->size = 0;
	string->buffer = NULL;
	val->value.string = string;
}

static void _freeString(struct mScriptValue* val) {
	struct mScriptString* string = val->value.string;
	if (string->buffer) {
		free(string->buffer);
	}
	free(string);
}

static bool _stringCast(const struct mScriptValue* in, const struct mScriptType* type, struct mScriptValue* out) {
	if (in->type == type) {
		out->type = type;
		out->refs = mSCRIPT_VALUE_UNREF;
		out->flags = 0;
		out->value.opaque = in->value.opaque;
		return true;
	}
	if (in->type == mSCRIPT_TYPE_MS_STR && type == mSCRIPT_TYPE_MS_CHARP) {
		out->type = type;
		out->refs = mSCRIPT_VALUE_UNREF;
		out->flags = 0;
		out->value.opaque = in->value.string->buffer;
		return true;
	}
	return false;
}

static uint32_t _hashString(const struct mScriptValue* val) {
	const char* buffer = 0;
	size_t size = 0;
	if (val->type == &mSTString) {
		struct mScriptString* string = val->value.string;
		buffer = string->buffer;
		size = string->size;
	} else if (val->type == &mSTCharPtr) {
		buffer = val->value.opaque;
		size = strlen(buffer);
	}
	return hash32(buffer, size, 0);
}

bool _wstrCast(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) {
	if (input->type->base != mSCRIPT_TYPE_WRAPPER) {
		return false;
	}
	const struct mScriptValue* unwrapped = mScriptValueUnwrapConst(input);
	if (unwrapped->type != mSCRIPT_TYPE_MS_STR) {
		return false;
	}
	memcpy(output, input, sizeof(*output));
	output->type = type;
	return true;
}

bool _wlistCast(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) {
	if (input->type->base != mSCRIPT_TYPE_WRAPPER) {
		return false;
	}
	const struct mScriptValue* unwrapped = mScriptValueUnwrapConst(input);
	if (unwrapped->type != mSCRIPT_TYPE_MS_LIST) {
		return false;
	}
	memcpy(output, input, sizeof(*output));
	output->type = type;
	return true;
}

bool _wtableCast(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) {
	if (input->type->base != mSCRIPT_TYPE_WRAPPER) {
		return false;
	}
	const struct mScriptValue* unwrapped = mScriptValueUnwrapConst(input);
	if (unwrapped->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	memcpy(output, input, sizeof(*output));
	output->type = type;
	return true;
}

#define AS(NAME, TYPE) \
	bool _as ## NAME(const struct mScriptValue* input, mSCRIPT_TYPE_C_ ## TYPE * T) { \
		switch (input->type->base) { \
		case mSCRIPT_TYPE_SINT: \
			if (input->type->size <= 4) { \
				*T = input->value.s32; \
			} else if (input->type->size == 8) { \
				*T = input->value.s64; \
			} else { \
				return false; \
			}\
			break; \
		case mSCRIPT_TYPE_UINT: \
			if (input->type->size <= 4) { \
				*T = input->value.u32; \
			} else if (input->type->size == 8) { \
				*T = input->value.u64; \
			} else { \
				return false; \
			} \
			break; \
		case mSCRIPT_TYPE_FLOAT: \
			if (input->type->size == 4) { \
				*T = input->value.f32; \
			} else if (input->type->size == 8) { \
				*T = input->value.f64; \
			} else { \
				return false; \
			} \
			break; \
		default: \
			return false; \
		} \
		return true; \
	}

AS(SInt32, S32);
AS(UInt32, U32);
AS(Float32, F32);
AS(SInt64, S64);
AS(UInt64, U64);
AS(Float64, F64);
AS(Bool, BOOL);

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
		if (type == mSCRIPT_TYPE_MS_BOOL) {
			bool b;
			if (!_asBool(input, &b)) {
				return false;
			}
			output->value.u32 = b;
		} else if (type->size <= 4) {
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

uint32_t _hashScalar(const struct mScriptValue* val) {
	// From https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
	uint32_t x = 0;
	_asUInt32(val, &x);
	x = ((x >> 16) ^ x) * 0x45D9F3B;
	x = ((x >> 16) ^ x) * 0x45D9F3B;
	x = (x >> 16) ^ x;
	return x;
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
		if (b->type == mSCRIPT_TYPE_MS_BOOL) {
			return !!a->value.s32 == b->value.u32;
		}
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
		if (b->type == mSCRIPT_TYPE_MS_BOOL) {
			return !!a->value.u32 == b->value.u32;
		}
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
	case mSCRIPT_TYPE_UINT:
		if (b->type == mSCRIPT_TYPE_MS_BOOL) {
			return (!(uint32_t) !a->value.f32)== b->value.u32;
		}
		// Fall through
	case mSCRIPT_TYPE_SINT:
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
		if (b->type == mSCRIPT_TYPE_MS_BOOL) {
			return !!a->value.s64 == b->value.u32;
		}
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
		if (b->type == mSCRIPT_TYPE_MS_BOOL) {
			return !!a->value.u64 == b->value.u32;
		}
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
	case mSCRIPT_TYPE_UINT:
		if (b->type == mSCRIPT_TYPE_MS_BOOL) {
			return (!(uint32_t) !a->value.f64)== b->value.u32;
		}
		// Fall through
	case mSCRIPT_TYPE_SINT:
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

bool _boolEqual(const struct mScriptValue* a, const struct mScriptValue* b) {
	switch (b->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (b->type->size <= 4) {
			return a->value.u32 == !!b->value.s32;
		} else if (b->type->size == 8) {
			return a->value.u32 == !!b->value.s64;
		}
		return false;
	case mSCRIPT_TYPE_UINT:
		if (b->type->size <= 4) {
			return a->value.u32 == !!b->value.u32;
		} else if (b->type->size == 8) {
			return a->value.u32 == !!b->value.u64;
		}
		return false;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
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
		struct mScriptString* stringB = b->value.string;
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
	struct mScriptString* stringA = a->value.string;
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
		struct mScriptString* stringB = b->value.string;
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
	val->flags = 0;
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
	} else if (val->flags & mSCRIPT_VALUE_FLAG_FREE_BUFFER) {
		free(val->value.opaque);
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
}

struct mScriptValue* mScriptValueUnwrap(struct mScriptValue* value) {
	if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
		return value->value.opaque;
	}
	return NULL;
}

const struct mScriptValue* mScriptValueUnwrapConst(const struct mScriptValue* value) {
	if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
		return value->value.copaque;
	}
	return NULL;
}

void mScriptValueFollowPointer(struct mScriptValue* ptr, struct mScriptValue* out) {
	if (ptr->type->base != mSCRIPT_TYPE_OPAQUE || !ptr->type->details.type) {
		return;
	}

	out->value.opaque = *(void**) ptr->value.opaque;
	if (out->value.opaque) {
		out->type = ptr->type->details.type;
	} else {
		out->type = mSCRIPT_TYPE_MS_VOID;
	}
	out->refs = mSCRIPT_VALUE_UNREF;
	out->flags = 0;
}

struct mScriptValue* mScriptStringCreateEmpty(size_t size) {
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_STR);
	struct mScriptString* internal = val->value.opaque;
	internal->size = size;
	internal->length = 0;
	internal->buffer = malloc(size + 1);
	memset(internal->buffer, 0, size + 1);
	return val;
}

struct mScriptValue* mScriptStringCreateFromBytes(const void* string, size_t size) {
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_STR);
	struct mScriptString* internal = val->value.opaque;
	internal->size = size;
	internal->length = 0;
	internal->buffer = malloc(size + 1);
	memcpy(internal->buffer, string, size);
	internal->buffer[size] = '\0';
	return val;
}

struct mScriptValue* mScriptStringCreateFromUTF8(const char* string) {
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_STR);
	struct mScriptString* internal = val->value.string;
	internal->size = strlen(string);
	internal->length = utf8strlen(string);
	internal->buffer = strdup(string);
	return val;
}

struct mScriptValue* mScriptStringCreateFromASCII(const char* string) {
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_STR);
	struct mScriptString* internal = val->value.string;
	internal->size = strlen(string);
	internal->length = strlen(string);
	internal->buffer = latin1ToUtf8(string, internal->size + 1);
	return val;
}

struct mScriptValue* mScriptValueCreateFromSInt(int32_t value) {
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
	val->value.s32 = value;
	return val;
}

struct mScriptValue* mScriptValueCreateFromUInt(uint32_t value) {
	struct mScriptValue* val = mScriptValueAlloc(mSCRIPT_TYPE_MS_U32);
	val->value.u32 = value;
	return val;
}

bool mScriptTableInsert(struct mScriptValue* table, struct mScriptValue* key, struct mScriptValue* value) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	if (!key->type->hash) {
		return false;
	}
	mScriptValueRef(value);
	HashTableInsertCustom(table->value.table, key, value);
	return true;
}

bool mScriptTableRemove(struct mScriptValue* table, struct mScriptValue* key) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	if (!key->type->hash) {
		return false;
	}
	HashTableRemoveCustom(table->value.table, key);
	return true;
}

struct mScriptValue* mScriptTableLookup(struct mScriptValue* table, struct mScriptValue* key) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return NULL;
	}
	if (!key->type->hash) {
		return NULL;
	}
	return HashTableLookupCustom(table->value.table, key);
}

bool mScriptTableClear(struct mScriptValue* table) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	HashTableClear(table->value.table);
	return true;
}

size_t mScriptTableSize(struct mScriptValue* table) {
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return 0;
	}
	return HashTableSize(table->value.table);
}

bool mScriptTableIteratorStart(struct mScriptValue* table, struct TableIterator* iter) {
	if (table->type->base == mSCRIPT_TYPE_WRAPPER) {
		table = mScriptValueUnwrap(table);
	}
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	return HashTableIteratorStart(table->value.table, iter);
}

bool mScriptTableIteratorNext(struct mScriptValue* table, struct TableIterator* iter) {
	if (table->type->base == mSCRIPT_TYPE_WRAPPER) {
		table = mScriptValueUnwrap(table);
	}
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	return HashTableIteratorNext(table->value.table, iter);
}

struct mScriptValue* mScriptTableIteratorGetKey(struct mScriptValue* table, struct TableIterator* iter) {
	if (table->type->base == mSCRIPT_TYPE_WRAPPER) {
		table = mScriptValueUnwrap(table);
	}
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return NULL;
	}
	return HashTableIteratorGetCustomKey(table->value.table, iter);
}

struct mScriptValue* mScriptTableIteratorGetValue(struct mScriptValue* table, struct TableIterator* iter) {
	if (table->type->base == mSCRIPT_TYPE_WRAPPER) {
		table = mScriptValueUnwrap(table);
	}
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return NULL;
	}
	return HashTableIteratorGetValue(table->value.table, iter);
}

bool mScriptTableIteratorLookup(struct mScriptValue* table, struct TableIterator* iter, struct mScriptValue* key) {
	if (table->type->base == mSCRIPT_TYPE_WRAPPER) {
		table = mScriptValueUnwrap(table);
	}
	if (table->type != mSCRIPT_TYPE_MS_TABLE) {
		return false;
	}
	return HashTableIteratorLookupCustom(table->value.table, iter, key);
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

	size_t i;
	for (i = 0; details[i].type != mSCRIPT_CLASS_INIT_END; ++i) {
		const struct mScriptClassInitDetails* detail = &details[i];
		struct mScriptClassMember* member;

		switch (detail->type) {
		case mSCRIPT_CLASS_INIT_END:
			break;
		case mSCRIPT_CLASS_INIT_CLASS_DOCSTRING:
			if (!child) {
				cls->docstring = detail->info.comment;
			}
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
		case mSCRIPT_CLASS_INIT_CAST_TO_MEMBER:
			HashTableInsert(&cls->castToMembers, detail->info.castMember.type->name, (char*) detail->info.castMember.member);
			break;
		case mSCRIPT_CLASS_INIT_INIT:
			cls->alloc = calloc(1, sizeof(*member));
			memcpy(cls->alloc, &detail->info.member, sizeof(*member));
			if (docstring) {
				cls->alloc->docstring = docstring;
				docstring = NULL;
			}
			break;
		case mSCRIPT_CLASS_INIT_DEINIT:
			cls->free = calloc(1, sizeof(*member));
			memcpy(cls->free, &detail->info.member, sizeof(*member));
			if (docstring) {
				cls->free->docstring = docstring;
				docstring = NULL;
			}
			break;
		case mSCRIPT_CLASS_INIT_GET:
			cls->get = calloc(1, sizeof(*member));
			memcpy(cls->get, &detail->info.member, sizeof(*member));
			if (docstring) {
				cls->get->docstring = docstring;
				docstring = NULL;
			}
			break;
		case mSCRIPT_CLASS_INIT_SET:
			member = calloc(1, sizeof(*member));
			memcpy(member, &detail->info.member, sizeof(*member));
			if (docstring) {
				member->docstring = docstring;
				docstring = NULL;
			}
			if (detail->info.member.type->details.function.parameters.count != 3) {
				abort();
			}
			HashTableInsert(&cls->setters, detail->info.member.type->details.function.parameters.entries[2]->name, member);
			break;
		case mSCRIPT_CLASS_INIT_INTERNAL:
			cls->internal = true;
			break;
		}
	}
}

void mScriptClassInit(struct mScriptTypeClass* cls) {
	if (cls->init) {
		return;
	}
	HashTableInit(&cls->instanceMembers, 0, free);
	HashTableInit(&cls->castToMembers, 0, NULL);
	HashTableInit(&cls->setters, 0, free);

	cls->alloc = NULL;
	cls->free = NULL;
	cls->get = NULL;
	_mScriptClassInit(cls, cls->details, false);

	cls->init = true;
}

void mScriptClassDeinit(struct mScriptTypeClass* cls) {
	if (!cls->init) {
		return;
	}
	HashTableDeinit(&cls->instanceMembers);
	HashTableDeinit(&cls->castToMembers);
	HashTableDeinit(&cls->setters);
	cls->init = false;
}


static bool _accessRawMember(struct mScriptClassMember* member, void* raw, bool isConst, struct mScriptValue* val) {
	raw = (void*) ((uintptr_t) raw + member->offset);
	switch (member->type->base) {
	case mSCRIPT_TYPE_SINT:
		switch (member->type->size) {
		case 1:
			*val = mSCRIPT_MAKE_S32(*(int8_t *) raw);
			break;
		case 2:
			*val = mSCRIPT_MAKE_S32(*(int16_t *) raw);
			break;
		case 4:
			*val = mSCRIPT_MAKE_S32(*(int32_t *) raw);
			break;
		case 8:
			*val = mSCRIPT_MAKE_S64(*(int64_t *) raw);
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		switch (member->type->size) {
		case 1:
			*val = mSCRIPT_MAKE_U32(*(uint8_t *) raw);
			break;
		case 2:
			*val = mSCRIPT_MAKE_U32(*(uint16_t *) raw);
			break;
		case 4:
			*val = mSCRIPT_MAKE_U32(*(uint32_t *) raw);
			break;
		case 8:
			*val = mSCRIPT_MAKE_U64(*(uint64_t *) raw);
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_FLOAT:
		switch (member->type->size) {
		case 4:
			*val = mSCRIPT_MAKE_F32(*(mSCRIPT_TYPE_C_F32 *) raw);
			break;
		case 8:
			*val = mSCRIPT_MAKE_F64(*(mSCRIPT_TYPE_C_F64 *) raw);
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_TABLE:
		val->refs = mSCRIPT_VALUE_UNREF;
		val->flags = 0;
		val->type = mSCRIPT_TYPE_MS_WRAPPER;
		val->value.table = raw;
		break;
	case mSCRIPT_TYPE_STRING:
		if (member->type == mSCRIPT_TYPE_MS_CHARP) {
			val->refs = mSCRIPT_VALUE_UNREF;
			val->flags = 0;
			val->type = mSCRIPT_TYPE_MS_CHARP;
			val->value.opaque = raw;
			break;
		}
		return false;
	case mSCRIPT_TYPE_LIST:
		val->refs = mSCRIPT_VALUE_UNREF;
		val->flags = 0;
		val->type = mSCRIPT_TYPE_MS_LIST;
		val->value.list = raw;
		break;
	case mSCRIPT_TYPE_FUNCTION:
		val->refs = mSCRIPT_VALUE_UNREF;
		val->flags = 0;
		val->type = member->type;
		member->type->alloc(val);
		break;
	case mSCRIPT_TYPE_OBJECT:
		val->refs = mSCRIPT_VALUE_UNREF;
		val->flags = 0;
		val->value.opaque = raw;
		if (isConst && !member->type->isConst) {
			val->type = member->type->constType;
		} else {
			val->type = member->type;
		}
		break;
	case mSCRIPT_TYPE_OPAQUE:
		val->refs = mSCRIPT_VALUE_UNREF;
		val->flags = 0;
		val->value.opaque = raw;
		val->type = member->type;
		break;
	default:
		return false;
	}
	return true;
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
		struct mScriptValue getMember;
		m = cls->get;
		if (!m || !_accessRawMember(m, obj->value.opaque, obj->type->isConst, &getMember)) {
			return false;
		}
		struct mScriptFrame frame;
		mScriptFrameInit(&frame);
		struct mScriptValue* this = mScriptListAppend(&frame.arguments);
		this->type = obj->type;
		this->refs = mSCRIPT_VALUE_UNREF;
		this->flags = 0;
		this->value.opaque = obj->value.opaque;
		mSCRIPT_PUSH(&frame.arguments, CHARP, member);
		if (!mScriptInvoke(&getMember, &frame) || mScriptListSize(&frame.returnValues) != 1) {
			mScriptFrameDeinit(&frame);
			return false;
		}
		memcpy(val, mScriptListGetPointer(&frame.returnValues, 0), sizeof(*val));
		mScriptFrameDeinit(&frame);
		return true;
	}

	return _accessRawMember(m, obj->value.opaque, obj->type->isConst, val);
}

bool mScriptObjectGetConst(const struct mScriptValue* obj, const char* member, struct mScriptValue* val) {
	if (obj->type->base == mSCRIPT_TYPE_WRAPPER) {
		obj = mScriptValueUnwrapConst(obj);
	}
	if (obj->type->base != mSCRIPT_TYPE_OBJECT) {
		return false;
	}

	const struct mScriptTypeClass* cls = obj->type->details.cls;
	if (!cls) {
		return false;
	}

	struct mScriptClassMember* m = HashTableLookup(&cls->instanceMembers, member);
	if (!m) {
		return false;
	}

	return _accessRawMember(m, obj->value.opaque, true, val);
}

static struct mScriptClassMember* _findSetter(const struct mScriptTypeClass* cls, const struct mScriptType* type) {
	struct mScriptClassMember* m = HashTableLookup(&cls->setters, type->name);
	if (m) {
		return m;
	}
	
	switch (type->base) {
	case mSCRIPT_TYPE_SINT:
		if (type->size < 2) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_S16->name);
			if (m) {
				return m;
			}
		}
		if (type->size < 4) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_S32->name);
			if (m) {
				return m;
			}
		}
		if (type->size < 8) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_S64->name);
			if (m) {
				return m;
			}
		}
		break;
	case mSCRIPT_TYPE_UINT:
		if (type->size < 2) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_U16->name);
			if (m) {
				return m;
			}
		}
		if (type->size < 4) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_U32->name);
			if (m) {
				return m;
			}
		}
		if (type->size < 8) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_U64->name);
			if (m) {
				return m;
			}
		}
		break;
	case mSCRIPT_TYPE_FLOAT:
		if (type->size < 8) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_F64->name);
			if (m) {
				return m;
			}
		}
		break;
	case mSCRIPT_TYPE_STRING:
		if (type == mSCRIPT_TYPE_MS_STR) {
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_CHARP->name);
			if (m) {
				return m;
			}
			m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_WSTR->name);
			if (m) {
				return m;
			}
		}
		break;
	case mSCRIPT_TYPE_LIST:
		m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_WLIST->name);
		if (m) {
			return m;
		}
		break;
	case mSCRIPT_TYPE_TABLE:
		m = HashTableLookup(&cls->setters, mSCRIPT_TYPE_MS_WTABLE->name);
		if (m) {
			return m;
		}
		break;
	default:
		break;
	}
	return NULL;
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
		if (val->type->base == mSCRIPT_TYPE_WRAPPER) {
			val = mScriptValueUnwrap(val);
		}
		struct mScriptValue setMember;
		m = _findSetter(cls, val->type);
		if (!m || !_accessRawMember(m, obj->value.opaque, obj->type->isConst, &setMember)) {
			return false;
		}
		struct mScriptFrame frame;
		mScriptFrameInit(&frame);
		struct mScriptValue* this = mScriptListAppend(&frame.arguments);
		this->type = obj->type;
		this->refs = mSCRIPT_VALUE_UNREF;
		this->flags = 0;
		this->value.opaque = obj->value.opaque;
		mSCRIPT_PUSH(&frame.arguments, CHARP, member);
		mScriptValueWrap(val, mScriptListAppend(&frame.arguments));
		if (!mScriptInvoke(&setMember, &frame) || mScriptListSize(&frame.returnValues) != 0) {
			mScriptFrameDeinit(&frame);
			return false;
		}
		mScriptFrameDeinit(&frame);
		return true;
	}

	if (m->readonly) {
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

bool mScriptObjectCast(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) {
	if (input->type == type || (input->type->constType == type)) {
		output->type = type;
		output->value.opaque = input->value.opaque;
		output->refs = mSCRIPT_VALUE_UNREF;
		output->flags = 0;
		return true;
	}
	if (input->type->base != mSCRIPT_TYPE_OBJECT) {
		return false;
	}
	const char* member = HashTableLookup(&input->type->details.cls->castToMembers, type->name);
	if (member) {
		struct mScriptValue cast;
		if (!mScriptObjectGetConst(input, member, &cast)) {
			return false;
		}
		if (cast.type == type) {
			memcpy(output, &cast, sizeof(*output));
			return true;
		}
		return mScriptCast(type, &cast, output);
	}
	return false;
}

void mScriptObjectFree(struct mScriptValue* value) {
	if (value->type->base != mSCRIPT_TYPE_OBJECT) {
		return;
	}
	if (value->flags & (mSCRIPT_VALUE_FLAG_DEINIT | mSCRIPT_VALUE_FLAG_FREE_BUFFER)) {
		mScriptClassInit(value->type->details.cls);
		if (value->type->details.cls->free) {
			struct mScriptValue deinitMember;
			if (_accessRawMember(value->type->details.cls->free, value->value.opaque, value->type->isConst, &deinitMember)) {
				struct mScriptFrame frame;
				mScriptFrameInit(&frame);
				struct mScriptValue* this = mScriptListAppend(&frame.arguments);
				this->type = mSCRIPT_TYPE_MS_WRAPPER;
				this->refs = mSCRIPT_VALUE_UNREF;
				this->flags = 0;
				this->value.opaque = value;
				mScriptInvoke(&deinitMember, &frame);
				mScriptFrameDeinit(&frame);
			}
		}
	}
	if (value->flags & mSCRIPT_VALUE_FLAG_FREE_BUFFER) {
		free(value->value.opaque);
	}
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

bool mScriptPopBool(struct mScriptList* list, bool* out) {
	mSCRIPT_POP(list, BOOL, val);
	*out = val;
	return true;
}

bool mScriptPopPointer(struct mScriptList* list, void** out) {
	mSCRIPT_POP(list, PTR, val);
	*out = val;
	return true;
}

bool mScriptCast(const struct mScriptType* type, const struct mScriptValue* input, struct mScriptValue* output) {
	if (input->type->base == mSCRIPT_TYPE_WRAPPER && type->base != mSCRIPT_TYPE_WRAPPER) {
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
	if (types->count < mScriptListSize(frame) && !types->variable) {
		return false;
	}
	if (types->count > mScriptListSize(frame) && !types->variable && !types->defaults) {
		return false;
	}
	size_t i;
	for (i = 0; i < mScriptListSize(frame) && i < types->count; ++i) {
		if (types->entries[i] == mScriptListGetPointer(frame, i)->type) {
			continue;
		}
		struct mScriptValue* unwrapped = NULL;
		if (mScriptListGetPointer(frame, i)->type->base == mSCRIPT_TYPE_WRAPPER) {
			unwrapped = mScriptValueUnwrap(mScriptListGetPointer(frame, i));
			if (types->entries[i] == unwrapped->type) {
				continue;
			}
		}
		if (!mScriptCast(types->entries[i], mScriptListGetPointer(frame, i), mScriptListGetPointer(frame, i))) {
			return false;
		}
	}
	if (types->variable) {
		return true;
	}

	for (; i < types->count; ++i) {
		if (!types->defaults[i].type) {
			return false;
		}
		memcpy(mScriptListAppend(frame), &types->defaults[i], sizeof(struct mScriptValue));
	}
	return true;
}

static void addTypesFromTuple(struct Table* types, const struct mScriptTypeTuple* tuple) {
	size_t i;
	for (i = 0; i < tuple->count; ++i) {
		mScriptTypeAdd(types, tuple->entries[i]);
	}
}

static void addTypesFromTable(struct Table* types, struct Table* table) {
	struct TableIterator iter;
	if (!HashTableIteratorStart(table, &iter)) {
		return;
	}
	do {
		struct mScriptClassMember* member = HashTableIteratorGetValue(table, &iter);
		mScriptTypeAdd(types, member->type);
	} while(HashTableIteratorNext(table, &iter));
}

void mScriptTypeAdd(struct Table* types, const struct mScriptType* type) {
	if (HashTableLookup(types, type->name) || type->isConst) {
		return;
	}
	HashTableInsert(types, type->name, (struct mScriptType*) type);
	switch (type->base) {
	case mSCRIPT_TYPE_FUNCTION:
		addTypesFromTuple(types, &type->details.function.parameters);
		addTypesFromTuple(types, &type->details.function.returnType);
		break;
	case mSCRIPT_TYPE_OBJECT:
		mScriptClassInit(type->details.cls);
		if (type->details.cls->parent) {
			mScriptTypeAdd(types, type->details.cls->parent);
		}
		addTypesFromTable(types, &type->details.cls->instanceMembers);
		break;
	case mSCRIPT_TYPE_OPAQUE:
	case mSCRIPT_TYPE_WRAPPER:
		if (type->details.type) {
			mScriptTypeAdd(types, type->details.type);
		}
	case mSCRIPT_TYPE_VOID:
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_FLOAT:
	case mSCRIPT_TYPE_STRING:
	case mSCRIPT_TYPE_LIST:
	case mSCRIPT_TYPE_TABLE:
	case mSCRIPT_TYPE_WEAKREF:
		// No subtypes
		break;
	}
}
