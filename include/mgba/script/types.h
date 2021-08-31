/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_TYPES_H
#define M_SCRIPT_TYPES_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/vector.h>

#define _mAPPLY(...) __VA_ARGS__

#define mSCRIPT_VALUE_UNREF -1
#define mSCRIPT_PARAMS_MAX 8

#define mSCRIPT_TYPE_C_S32 int32_t
#define mSCRIPT_TYPE_C_U32 uint32_t
#define mSCRIPT_TYPE_C_F32 float
#define mSCRIPT_TYPE_C_STR mScriptString*
#define mSCRIPT_TYPE_C_CHARP const char*
#define mSCRIPT_TYPE_C_PTR void*
#define mSCRIPT_TYPE_C_TABLE Table*
#define mSCRIPT_TYPE_C_S(STRUCT) struct STRUCT*

#define mSCRIPT_TYPE_FIELD_S32 s32
#define mSCRIPT_TYPE_FIELD_U32 u32
#define mSCRIPT_TYPE_FIELD_F32 f32
#define mSCRIPT_TYPE_FIELD_STR opaque
#define mSCRIPT_TYPE_FIELD_CHARP opaque
#define mSCRIPT_TYPE_FIELD_PTR opaque
#define mSCRIPT_TYPE_FIELD_TABLE opaque
#define mSCRIPT_TYPE_FIELD_S(STRUCT) opaque

#define mSCRIPT_TYPE_MS_S32 (&mSTSInt32)
#define mSCRIPT_TYPE_MS_U32 (&mSTUInt32)
#define mSCRIPT_TYPE_MS_F32 (&mSTFloat32)
#define mSCRIPT_TYPE_MS_STR (&mSTString)
#define mSCRIPT_TYPE_MS_CHARP (&mSTCharPtr)
#define mSCRIPT_TYPE_MS_TABLE (&mSTTable)
#define mSCRIPT_TYPE_MS_S(STRUCT) (&mSTStruct_ ## STRUCT)

#define _mSCRIPT_FIELD_NAME(V) (V)->name

#define mSCRIPT_TYPE_CMP_GENERIC(TYPE0, TYPE1) (TYPE0 == TYPE1)
#define mSCRIPT_TYPE_CMP_U32(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_U32, TYPE)
#define mSCRIPT_TYPE_CMP_S32(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_S32, TYPE)
#define mSCRIPT_TYPE_CMP_F32(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_F32, TYPE)
#define mSCRIPT_TYPE_CMP_STR(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_STR, TYPE)
#define mSCRIPT_TYPE_CMP_CHARP(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_CHARP, TYPE)
#define mSCRIPT_TYPE_CMP_PTR(TYPE) ((TYPE)->base >= mSCRIPT_TYPE_OPAQUE)
#define mSCRIPT_TYPE_CMP_S(STRUCT) mSCRIPT_TYPE_MS_S(STRUCT)->name == _mSCRIPT_FIELD_NAME
#define mSCRIPT_TYPE_CMP(TYPE0, TYPE1) _mAPPLY(mSCRIPT_TYPE_CMP_ ## TYPE0(TYPE1))

#define mSCRIPT_POP(STACK, TYPE, NAME) \
	_mAPPLY(mSCRIPT_TYPE_C_ ## TYPE) NAME; \
	do { \
		struct mScriptValue* _val = mScriptListGetPointer(STACK, mScriptListSize(STACK) - 1); \
		if (!(mSCRIPT_TYPE_CMP(TYPE, _val->type))) { \
			return false; \
		} \
		NAME = _val->value. _mAPPLY(mSCRIPT_TYPE_FIELD_ ## TYPE); \
		mScriptListResize(STACK, -1); \
	} while (0)

#define mSCRIPT_POP_0(...)
#define mSCRIPT_POP_1(FRAME, T0) mSCRIPT_POP(FRAME, T0, p0)
#define mSCRIPT_POP_2(FRAME, T0, T1) mSCRIPT_POP(FRAME, T1, p1); mSCRIPT_POP_1(FRAME, T0)
#define mSCRIPT_POP_3(FRAME, T0, T1, T2) mSCRIPT_POP(FRAME, T2, p2); mSCRIPT_POP_2(FRAME, T0, T1)
#define mSCRIPT_POP_4(FRAME, T0, T1, T2, T3) mSCRIPT_POP(FRAME, T3, p3); mSCRIPT_POP_3(FRAME, T0, T1, T2)
#define mSCRIPT_POP_5(FRAME, T0, T1, T2, T3, T4) mSCRIPT_POP(FRAME, T4, p4); mSCRIPT_POP_4(FRAME, T0, T1, T2, T3)
#define mSCRIPT_POP_6(FRAME, T0, T1, T2, T3, T4, T5) mSCRIPT_POP(FRAME, T5, p5); mSCRIPT_POP_5(FRAME, T0, T1, T2, T3, T4)
#define mSCRIPT_POP_7(FRAME, T0, T1, T2, T3, T4, T5, T6) mSCRIPT_POP(FRAME, T6, p6); mSCRIPT_POP_6(FRAME, T0, T1, T2, T3, T4, T5)
#define mSCRIPT_POP_8(FRAME, T0, T1, T2, T3, T4, T5, T6, T7) mSCRIPT_POP(FRAME, T7, p7); mSCRIPT_POP_7(FRAME, T0, T1, T2, T3, T4, T5, T6)

#define mSCRIPT_PUSH(STACK, TYPE, NAME) \
	do { \
		struct mScriptValue* _val = mScriptListAppend(STACK); \
		_val->type = _mAPPLY(mSCRIPT_TYPE_MS_ ## TYPE); \
		_val->refs = mSCRIPT_VALUE_UNREF; \
		_val->value._mAPPLY(mSCRIPT_TYPE_FIELD_ ## TYPE) = NAME; \
	} while (0)

#define mSCRIPT_ARG_NAMES_0
#define mSCRIPT_ARG_NAMES_1 p0
#define mSCRIPT_ARG_NAMES_2 p0, p1
#define mSCRIPT_ARG_NAMES_3 p0, p1, p2
#define mSCRIPT_ARG_NAMES_4 p0, p1, p2, p3
#define mSCRIPT_ARG_NAMES_5 p0, p1, p2, p3, p4
#define mSCRIPT_ARG_NAMES_6 p0, p1, p2, p3, p4, p5
#define mSCRIPT_ARG_NAMES_7 p0, p1, p2, p3, p4, p5, p6
#define mSCRIPT_ARG_NAMES_8 p0, p1, p2, p3, p4, p5, p6, p7

#define mSCRIPT_PREFIX_0(PREFIX, ...)
#define mSCRIPT_PREFIX_1(PREFIX, T0) PREFIX ## T0
#define mSCRIPT_PREFIX_2(PREFIX, T0, T1) PREFIX ## T0, PREFIX ## T1
#define mSCRIPT_PREFIX_3(PREFIX, T0, T1, T2) PREFIX ## T0, PREFIX ## T1, PREFIX ## T2
#define mSCRIPT_PREFIX_4(PREFIX, T0, T1, T2, T3) PREFIX ## T0, PREFIX ## T1, PREFIX ## T2, PREFIX ## T3
#define mSCRIPT_PREFIX_5(PREFIX, T0, T1, T2, T3, T4) PREFIX ## T0, PREFIX ## T1, PREFIX ## T2, PREFIX ## T3, PREFIX ## T4
#define mSCRIPT_PREFIX_6(PREFIX, T0, T1, T2, T3, T4, T5) PREFIX ## T0, PREFIX ## T1, PREFIX ## T2, PREFIX ## T3, PREFIX ## T4, PREFIX ## T5
#define mSCRIPT_PREFIX_7(PREFIX, T0, T1, T2, T3, T4, T5, T6) PREFIX ## T0, PREFIX ## T1, PREFIX ## T2, PREFIX ## T3, PREFIX ## T4, PREFIX ## T5, PREFIX ## T6
#define mSCRIPT_PREFIX_8(PREFIX, T0, T1, T2, T3, T4, T5, T6, T7) PREFIX ## T0, PREFIX ## T1, PREFIX ## T2, PREFIX ## T3, PREFIX ## T4, PREFIX ## T5, PREFIX ## T6, PREFIX ## T7

#define _mSCRIPT_CALL_VOID(FUNCTION, NPARAMS) FUNCTION(mSCRIPT_ARG_NAMES_ ## NPARAMS)
#define _mSCRIPT_CALL(RETURN, FUNCTION, NPARAMS) \
	_mAPPLY(mSCRIPT_TYPE_C_ ## RETURN) out = FUNCTION(mSCRIPT_ARG_NAMES_ ## NPARAMS); \
	mSCRIPT_PUSH(&frame->returnValues, RETURN, out)

#define mSCRIPT_EXPORT_STRUCT(STRUCT) \
	const struct mScriptType mSTStruct_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_OBJECT, \
		.size = sizeof(struct STRUCT), \
		.name = "struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
	}

#define mSCRIPT_BIND_FUNCTION(NAME, RETURN, FUNCTION, NPARAMS, ...) \
	static bool _binding_ ## NAME(struct mScriptFrame* frame) { \
		mSCRIPT_POP_ ## NPARAMS(&frame->arguments, __VA_ARGS__); \
		if (mScriptListSize(&frame->arguments)) { \
			return false; \
		} \
		_mSCRIPT_CALL(RETURN, FUNCTION, NPARAMS); \
		return true; \
	} \
	const struct mScriptFunction NAME = { \
		.signature = { \
			.parameters = { \
				.count = NPARAMS, \
				.entries = { _mAPPLY(mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_MS_, __VA_ARGS__)) } \
			}, \
			.returnType = { \
				.count = 1, \
				.entries = { mSCRIPT_TYPE_MS_ ## RETURN } \
			}, \
		}, \
		.call = _binding_ ## NAME \
	};

#define mSCRIPT_BIND_VOID_FUNCTION(NAME, FUNCTION, NPARAMS, ...) \
	static bool _binding_ ## NAME(struct mScriptFrame* frame) { \
		mSCRIPT_POP_ ## NPARAMS(&frame->arguments, __VA_ARGS__); \
		if (mScriptListSize(&frame->arguments)) { \
			return false; \
		} \
		_mSCRIPT_CALL_VOID(FUNCTION, NPARAMS); \
		return true; \
	} \
	const struct mScriptFunction NAME = { \
		.signature = { \
			.parameters = { \
				.count = NPARAMS, \
				.entries = { _mAPPLY(mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_MS_, __VA_ARGS__)) } \
			}, \
			.returnType = { \
				.count = 0, \
			}, \
		}, \
		.call = _binding_ ## NAME \
	};

#define mSCRIPT_MAKE(TYPE, FIELD, VALUE) (struct mScriptValue) { \
		.type = (TYPE), \
		.refs = mSCRIPT_VALUE_UNREF, \
		.value = { \
			.FIELD = (VALUE) \
		}, \
	} \

#define mSCRIPT_MAKE_S32(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_S32, s32, VALUE)
#define mSCRIPT_MAKE_U32(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_U32, u32, VALUE)
#define mSCRIPT_MAKE_F32(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_F32, f32, VALUE)
#define mSCRIPT_MAKE_CHARP(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_CHARP, opaque, VALUE)

enum {
	mSCRIPT_TYPE_VOID = 0,
	mSCRIPT_TYPE_SINT,
	mSCRIPT_TYPE_UINT,
	mSCRIPT_TYPE_FLOAT,
	mSCRIPT_TYPE_STRING,
	mSCRIPT_TYPE_FUNCTION,
	mSCRIPT_TYPE_OPAQUE,
	mSCRIPT_TYPE_OBJECT,
	mSCRIPT_TYPE_TUPLE,
	mSCRIPT_TYPE_LIST,
	mSCRIPT_TYPE_TABLE,
};

struct Table;
struct mScriptType;
extern const struct mScriptType mSTVoid;
extern const struct mScriptType mSTSInt32;
extern const struct mScriptType mSTUInt32;
extern const struct mScriptType mSTFloat32;
extern const struct mScriptType mSTString;
extern const struct mScriptType mSTCharPtr;
extern const struct mScriptType mSTTable;

struct mScriptTypeTuple {
	size_t count;
	const struct mScriptType* entries[mSCRIPT_PARAMS_MAX];
};

struct mScriptTypeFunction {
	struct mScriptTypeTuple parameters;
	struct mScriptTypeTuple returnType;
	// TODO: varargs, kwargs, defaults
};

struct mScriptValue;
struct mScriptType {
	int base;
	size_t size;
	const char* name;
	union {
		struct mScriptTypeTuple tuple;
		struct mScriptTypeFunction function;
		void* opaque;
	} details;
	void (*alloc)(struct mScriptValue*);
	void (*free)(struct mScriptValue*);
	uint32_t (*hash)(const struct mScriptValue*);
	bool (*equal)(const struct mScriptValue*, const struct mScriptValue*);
	bool (*cast)(const struct mScriptValue*, const struct mScriptType*, struct mScriptValue*);
};

struct mScriptValue {
	const struct mScriptType* type;
	int refs;
	union {
		int32_t s32;
		uint32_t u32;
		float f32;
		void* opaque;
	} value;
};

DECLARE_VECTOR(mScriptList, struct mScriptValue)

struct mScriptString {
	size_t length; // Size of the string in code points
	size_t size; // Size of the buffer in bytes, excluding NULL byte terminator
	char* buffer; // UTF-8 representation of the string
};

struct mScriptFrame {
	struct mScriptList arguments;
	struct mScriptList returnValues;
	// TODO: Exception/failure codes
};

struct mScriptFunction {
	struct mScriptTypeFunction signature;
	bool (*call)(struct mScriptFrame*);
	void* context;
};

struct mScriptValue* mScriptValueAlloc(const struct mScriptType* type);
void mScriptValueRef(struct mScriptValue* val);
void mScriptValueDeref(struct mScriptValue* val);

struct mScriptValue* mScriptStringCreateFromUTF8(const char* string);

bool mScriptTableInsert(struct mScriptValue* table, struct mScriptValue* key, struct mScriptValue* value);
bool mScriptTableRemove(struct mScriptValue* table, struct mScriptValue* key);
struct mScriptValue* mScriptTableLookup(struct mScriptValue* table, struct mScriptValue* key);

void mScriptFrameInit(struct mScriptFrame* frame);
void mScriptFrameDeinit(struct mScriptFrame* frame);

bool mScriptPopS32(struct mScriptList* list, int32_t* out);
bool mScriptPopU32(struct mScriptList* list, uint32_t* out);
bool mScriptPopF32(struct mScriptList* list, float* out);
bool mScriptPopPointer(struct mScriptList* list, void** out);

bool mScriptCast(const struct mScriptType* type, const struct mScriptValue* input, struct mScriptValue* output);
bool mScriptCoerceFrame(const struct mScriptTypeTuple* types, struct mScriptList* frame);
bool mScriptInvoke(const struct mScriptFunction* fn, struct mScriptFrame* frame);

#endif
