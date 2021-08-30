/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/types.h>

#include <mgba-util/table.h>

static void _allocTable(const struct mScriptType*, struct mScriptValue*);
static void _freeTable(const struct mScriptType*, struct mScriptValue*);
static void _deinitTableValue(void*);

static uint32_t _hashScalar(const struct mScriptType*, const struct mScriptValue*);

static uint32_t _valHash(const void* val, size_t len, uint32_t seed);
static bool _valEqual(const void* a, const void* b);
static void* _valRef(void*);
static void _valDeref(void*);

const struct mScriptType mSTVoid = {
	.base = mSCRIPT_TYPE_VOID,
	.size = 0,
	.name = "void",
	.alloc = NULL,
	.free = NULL,
	.hash = NULL,
};

const struct mScriptType mSTSInt32 = {
	.base = mSCRIPT_TYPE_SINT,
	.size = 4,
	.name = "s32",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
};

const struct mScriptType mSTUInt32 = {
	.base = mSCRIPT_TYPE_UINT,
	.size = 4,
	.name = "u32",
	.alloc = NULL,
	.free = NULL,
	.hash = _hashScalar,
};

const struct mScriptType mSTFloat32 = {
	.base = mSCRIPT_TYPE_FLOAT,
	.size = 4,
	.name = "f32",
	.alloc = NULL,
	.free = NULL,
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

DEFINE_VECTOR(mScriptList, struct mScriptValue)

void _allocTable(const struct mScriptType* type, struct mScriptValue* val) {
	UNUSED(type);
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

void _freeTable(const struct mScriptType* type, struct mScriptValue* val) {
	UNUSED(type);
	HashTableDeinit(val->value.opaque);
	free(val->value.opaque);
}

void _deinitTableValue(void* val) {
	mScriptValueDeref(val);
}

uint32_t _hashScalar(const struct mScriptType* type, const struct mScriptValue* val) {
	// From https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
	uint32_t x;
	switch (type->base) {
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

uint32_t _valHash(const void* val, size_t len, uint32_t seed) {
	UNUSED(len);
	const struct mScriptValue* value = val;
	uint32_t hash = value->type->hash(value->type, value);
	return hash ^ seed;
}

bool _valEqual(const void* a, const void* b) {
	const struct mScriptValue* valueA = a;
	const struct mScriptValue* valueB = b;
	// TODO: Move equality into type
	if (valueA->type != valueB->type) {
		return false;
	}
	switch (valueA->type->base) {
	case mSCRIPT_TYPE_VOID:
		return true;
	case mSCRIPT_TYPE_SINT:
		return valueA->value.s32 == valueB->value.s32;
	case mSCRIPT_TYPE_UINT:
		return valueA->value.u32 == valueB->value.u32;
	case mSCRIPT_TYPE_FLOAT:
		return valueA->value.f32 == valueB->value.f32;
	default:
		return valueA->value.opaque == valueB->value.opaque;
	}
}

void* _valRef(void* val) {
	mScriptValueRef(val);
	return val;
}

void _valDeref(void* val) {
	mScriptValueDeref(val);
}

struct mScriptValue* mScriptValueAlloc(const struct mScriptType* type) {
	// TODO: Use an arena instead of just the generic heap
	struct mScriptValue* val = malloc(sizeof(*val));
	val->refs = 1;
	if (type->alloc) {
		type->alloc(type, val);
	} else {
		val->value.opaque = NULL;
	}
	val->type = type;
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
		val->type->free(val->type, val);
	}
	free(val);
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
	switch (type->base) {
	case mSCRIPT_TYPE_VOID:
		return false;
	case mSCRIPT_TYPE_SINT:
		switch (input->type->base) {
		case mSCRIPT_TYPE_SINT:
			output->value.s32 = input->value.s32;
			break;
		case mSCRIPT_TYPE_UINT:
			output->value.s32 = input->value.u32;
			break;
		case mSCRIPT_TYPE_FLOAT:
			switch (input->type->size) {
			case 4:
				output->value.s32 = input->value.f32;
				break;
			default:
				return false;
			}
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
			switch (input->type->size) {
			case 4:
				output->value.u32 = input->value.f32;
				break;
			default:
				return false;
			}
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
			switch (input->type->size) {
			case 4:
				output->value.f32 = input->value.f32;
				break;
			default:
				return false;
			}
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
