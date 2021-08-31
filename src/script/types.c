/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/types.h>

#include <mgba-util/hash.h>
#include <mgba-util/table.h>

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
	uint32_t x;
	switch (val->type->base) {
	case mSCRIPT_TYPE_SINT:
		x = val->value.s32;
		break;
	case mSCRIPT_TYPE_UINT:
		x = val->value.u32;
		break;
	}
	x = ((x >> 16) ^ x) * 0x45D9F3B;
	x = ((x >> 16) ^ x) * 0x45D9F3B;
	x = (x >> 16) ^ x;
	return x;
}

bool _castScalar(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) {
	switch (type->base) {
	case mSCRIPT_TYPE_SINT:
		switch (input->type->base) {
		case mSCRIPT_TYPE_SINT:
			output->value.s32 = input->value.s32;
			break;
		case mSCRIPT_TYPE_UINT:
			output->value.s32 = input->value.u32;
			break;
		case mSCRIPT_TYPE_FLOAT:
			output->value.s32 = input->value.f32;
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		switch (input->type->base) {
		case mSCRIPT_TYPE_SINT:
			output->value.u32 = input->value.s32;
			break;
		case mSCRIPT_TYPE_UINT:
			output->value.u32 = input->value.u32;
			break;
		case mSCRIPT_TYPE_FLOAT:
			output->value.u32 = input->value.f32;
			break;
		default:
			return false;
		}
		break;
	case mSCRIPT_TYPE_FLOAT:
		switch (input->type->base) {
		case mSCRIPT_TYPE_SINT:
			output->value.f32 = input->value.s32;
			break;
		case mSCRIPT_TYPE_UINT:
			output->value.f32 = input->value.u32;
			break;
		case mSCRIPT_TYPE_FLOAT:
			output->value.f32 = input->value.f32;
			break;
		default:
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
		val = b->value.s32;
		break;
	case mSCRIPT_TYPE_UINT:
		if (b->value.u32 > (uint32_t) INT_MAX) {
			return false;
		}
		if (a->value.s32 < 0) {
			return false;
		}
		val = b->value.u32;
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
		if (b->value.s32 < 0) {
			return false;
		}
		if (a->value.u32 > (uint32_t) INT_MAX) {
			return false;
		}
		val = b->value.s32;
		break;
	case mSCRIPT_TYPE_UINT:
		val = b->value.u32;
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
		val = b->value.s32;
		break;
	case mSCRIPT_TYPE_UINT:
		val = b->value.u32;
		break;
	case mSCRIPT_TYPE_FLOAT:
		val = b->value.f32;
		break;
	case mSCRIPT_TYPE_VOID:
		return false;
	default:
		return b->type->equal && b->type->equal(b, a);
	}
	return a->value.f32 == val;
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
	--val->refs;
	if (val->refs > 0) {
		return;
	} else if (val->refs < 0) {
		val->refs = mSCRIPT_VALUE_UNREF;
		return;
	}
	if (val->type->free) {
		val->type->free(val);
	}
	free(val);
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

bool mScriptPopS32(struct mScriptList* list, int32_t* out) {
	mSCRIPT_POP(list, S32, val);
	*out = val;
	return true;
}

bool mScriptPopU32(struct mScriptList* list, uint32_t* out) {
	mSCRIPT_POP(list, U32, val);
	*out = val;
	return true;
}

bool mScriptPopF32(struct mScriptList* list, float* out) {
	mSCRIPT_POP(list, F32, val);
	*out = val;
	return true;
}

bool mScriptPopPointer(struct mScriptList* list, void** out) {
	mSCRIPT_POP(list, PTR, val);
	*out = val;
	return true;
}

bool mScriptCast(const struct mScriptType* type, const struct mScriptValue* input, struct mScriptValue* output) {
	if (type->cast && type->cast(input, type, output)) {
		return true;
	}
	if (input->type->cast && input->type->cast(input, type, output)) {
		return true;
	}
	return true;
}

bool mScriptCoerceFrame(const struct mScriptTypeTuple* types, struct mScriptList* frame) {
	if (types->count != mScriptListSize(frame)) {
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

bool mScriptInvoke(const struct mScriptFunction* fn, struct mScriptFrame* frame) {
	if (!mScriptCoerceFrame(&fn->signature.parameters, &frame->arguments)) {
		return false;
	}
	return fn->call(frame);
}
