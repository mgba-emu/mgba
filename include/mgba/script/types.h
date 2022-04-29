/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_TYPES_H
#define M_SCRIPT_TYPES_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/table.h>
#include <mgba-util/vector.h>

#define _mCPP_EMPTY()
#define _mCPP_CAT(A, B) A ## B

#define _mDEFER(X) X _mCPP_EMPTY()
#define _mBLOCK(...) __VA_ARGS__ _mDEFER(_mCPP_EMPTY)()
#define _mAPPLY(...) __VA_ARGS__
#define _mCAT(A, B) _mCPP_CAT(A, B)

#define mSCRIPT_VALUE_UNREF -1
#define mSCRIPT_PARAMS_MAX 8

#define mSCRIPT_TYPE_C_S8 int8_t
#define mSCRIPT_TYPE_C_U8 uint8_t
#define mSCRIPT_TYPE_C_S16 int16_t
#define mSCRIPT_TYPE_C_U16 uint16_t
#define mSCRIPT_TYPE_C_S32 int32_t
#define mSCRIPT_TYPE_C_U32 uint32_t
#define mSCRIPT_TYPE_C_F32 float
#define mSCRIPT_TYPE_C_S64 int64_t
#define mSCRIPT_TYPE_C_U64 uint64_t
#define mSCRIPT_TYPE_C_F64 double
#define mSCRIPT_TYPE_C_STR struct mScriptString*
#define mSCRIPT_TYPE_C_CHARP const char*
#define mSCRIPT_TYPE_C_PTR void*
#define mSCRIPT_TYPE_C_TABLE Table*
#define mSCRIPT_TYPE_C_WRAPPER struct mScriptValue*
#define mSCRIPT_TYPE_C_S(STRUCT) struct STRUCT*
#define mSCRIPT_TYPE_C_CS(STRUCT) const struct STRUCT*
#define mSCRIPT_TYPE_C_S_METHOD(STRUCT, NAME) _mSTStructFunctionType_ ## STRUCT ## _ ## NAME

#define mSCRIPT_TYPE_FIELD_S8 s32
#define mSCRIPT_TYPE_FIELD_U8 s32
#define mSCRIPT_TYPE_FIELD_S16 s32
#define mSCRIPT_TYPE_FIELD_U16 s32
#define mSCRIPT_TYPE_FIELD_S32 s32
#define mSCRIPT_TYPE_FIELD_U32 u32
#define mSCRIPT_TYPE_FIELD_F32 f32
#define mSCRIPT_TYPE_FIELD_S64 s64
#define mSCRIPT_TYPE_FIELD_U64 u64
#define mSCRIPT_TYPE_FIELD_F64 f64
#define mSCRIPT_TYPE_FIELD_STR opaque
#define mSCRIPT_TYPE_FIELD_CHARP opaque
#define mSCRIPT_TYPE_FIELD_PTR opaque
#define mSCRIPT_TYPE_FIELD_TABLE opaque
#define mSCRIPT_TYPE_FIELD_WRAPPER opaque
#define mSCRIPT_TYPE_FIELD_S(STRUCT) opaque
#define mSCRIPT_TYPE_FIELD_CS(STRUCT) copaque
#define mSCRIPT_TYPE_FIELD_S_METHOD(STRUCT, NAME) copaque

#define mSCRIPT_TYPE_MS_S8 (&mSTSInt8)
#define mSCRIPT_TYPE_MS_U8 (&mSTUInt8)
#define mSCRIPT_TYPE_MS_S16 (&mSTSInt16)
#define mSCRIPT_TYPE_MS_U16 (&mSTUInt16)
#define mSCRIPT_TYPE_MS_S32 (&mSTSInt32)
#define mSCRIPT_TYPE_MS_U32 (&mSTUInt32)
#define mSCRIPT_TYPE_MS_F32 (&mSTFloat32)
#define mSCRIPT_TYPE_MS_S64 (&mSTSInt64)
#define mSCRIPT_TYPE_MS_U64 (&mSTUInt64)
#define mSCRIPT_TYPE_MS_F64 (&mSTFloat64)
#define mSCRIPT_TYPE_MS_STR (&mSTString)
#define mSCRIPT_TYPE_MS_CHARP (&mSTCharPtr)
#define mSCRIPT_TYPE_MS_TABLE (&mSTTable)
#define mSCRIPT_TYPE_MS_WRAPPER (&mSTWrapper)
#define mSCRIPT_TYPE_MS_S(STRUCT) (&mSTStruct_ ## STRUCT)
#define mSCRIPT_TYPE_MS_CS(STRUCT) (&mSTStructConst_ ## STRUCT)
#define mSCRIPT_TYPE_MS_S_METHOD(STRUCT, NAME) (&_mSTStructBindingType_ ## STRUCT ## _ ## NAME)

#define _mSCRIPT_FIELD_NAME(V) (V)->name

#define mSCRIPT_TYPE_CMP_GENERIC(TYPE0, TYPE1) (TYPE0 == TYPE1)
#define mSCRIPT_TYPE_CMP_U8(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_U8, TYPE)
#define mSCRIPT_TYPE_CMP_S8(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_S8, TYPE)
#define mSCRIPT_TYPE_CMP_U16(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_U16, TYPE)
#define mSCRIPT_TYPE_CMP_S16(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_S16, TYPE)
#define mSCRIPT_TYPE_CMP_U32(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_U32, TYPE)
#define mSCRIPT_TYPE_CMP_S32(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_S32, TYPE)
#define mSCRIPT_TYPE_CMP_F32(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_F32, TYPE)
#define mSCRIPT_TYPE_CMP_U64(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_U64, TYPE)
#define mSCRIPT_TYPE_CMP_S64(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_S64, TYPE)
#define mSCRIPT_TYPE_CMP_F64(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_F64, TYPE)
#define mSCRIPT_TYPE_CMP_STR(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_STR, TYPE)
#define mSCRIPT_TYPE_CMP_CHARP(TYPE) mSCRIPT_TYPE_CMP_GENERIC(mSCRIPT_TYPE_MS_CHARP, TYPE)
#define mSCRIPT_TYPE_CMP_PTR(TYPE) ((TYPE)->base >= mSCRIPT_TYPE_OPAQUE)
#define mSCRIPT_TYPE_CMP_S(STRUCT) mSCRIPT_TYPE_MS_S(STRUCT)->name == _mSCRIPT_FIELD_NAME
#define mSCRIPT_TYPE_CMP_CS(STRUCT) mSCRIPT_TYPE_MS_CS(STRUCT)->name == _mSCRIPT_FIELD_NAME
#define mSCRIPT_TYPE_CMP_S_METHOD(STRUCT, NAME) mSCRIPT_TYPE_MS_S_METHOD(STRUCT, NAME)->name == _mSCRIPT_FIELD_NAME
#define mSCRIPT_TYPE_CMP(TYPE0, TYPE1) _mAPPLY(mSCRIPT_TYPE_CMP_ ## TYPE0(TYPE1))

#define mSCRIPT_POP(STACK, TYPE, NAME) \
	_mAPPLY(mSCRIPT_TYPE_C_ ## TYPE) NAME; \
	do { \
		struct mScriptValue* _val = mScriptListGetPointer(STACK, mScriptListSize(STACK) - 1); \
		if (!(mSCRIPT_TYPE_CMP(TYPE, _val->type))) { \
			if (_val->type == mSCRIPT_TYPE_MS_WRAPPER) { \
				_val = mScriptValueUnwrap(_val); \
				if (!(mSCRIPT_TYPE_CMP(TYPE, _val->type))) { \
					return false; \
				} \
			} else { \
				return false; \
			} \
		} \
		NAME = _val->value. _mAPPLY(mSCRIPT_TYPE_FIELD_ ## TYPE); \
		mScriptValueDeref(_val); \
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

#define _mCOMMA_0(N, ...) N
#define _mCOMMA_1(N, ...) N, __VA_ARGS__
#define _mCOMMA_2(N, ...) N, __VA_ARGS__
#define _mCOMMA_3(N, ...) N, __VA_ARGS__
#define _mCOMMA_4(N, ...) N, __VA_ARGS__
#define _mCOMMA_5(N, ...) N, __VA_ARGS__
#define _mCOMMA_6(N, ...) N, __VA_ARGS__
#define _mCOMMA_7(N, ...) N, __VA_ARGS__

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
#define mSCRIPT_PREFIX_N(N) _mAPPLY(mSCRIPT_PREFIX_ ## N)

#define _mSUCC0 1
#define _mSUCC1 2
#define _mSUCC2 3
#define _mSUCC3 4
#define _mSUCC4 5
#define _mSUCC5 6
#define _mSUCC6 7
#define _mSUCC7 8

#define _mPREC1 0
#define _mPREC2 1
#define _mPREC3 2
#define _mPREC4 3
#define _mPREC5 4
#define _mPREC6 5
#define _mPREC7 6
#define _mPREC8 7

#define _mSCRIPT_CALL_VOID(FUNCTION, NPARAMS) FUNCTION(_mCAT(mSCRIPT_ARG_NAMES_, NPARAMS))
#define _mSCRIPT_CALL(RETURN, FUNCTION, NPARAMS) \
	_mAPPLY(mSCRIPT_TYPE_C_ ## RETURN) out = FUNCTION(_mCAT(mSCRIPT_ARG_NAMES_, NPARAMS)); \
	mSCRIPT_PUSH(&frame->returnValues, RETURN, out)

#define mSCRIPT_EXPORT_STRUCT(STRUCT) \
	mSCRIPT_DECLARE_STRUCT(STRUCT) \
	static bool _mSTStructCast_ ## STRUCT(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) { \
		if (input->type == type || (input->type == &mSTStruct_ ## STRUCT && type == &mSTStructConst_ ## STRUCT)) { \
			output->type = type; \
			output->value.opaque = input->value.opaque; \
			return true; \
		} \
		return false; \
	} \
	const struct mScriptType mSTStruct_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_OBJECT, \
		.details = { \
			.cls = &_mSTStructDetails_ ## STRUCT \
		}, \
		.size = sizeof(struct STRUCT), \
		.name = "struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
		.cast = _mSTStructCast_ ## STRUCT, \
	}; \
	const struct mScriptType mSTStructConst_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_OBJECT, \
		.details = { \
			.cls = &_mSTStructDetails_ ## STRUCT \
		}, \
		.size = sizeof(struct STRUCT), \
		.name = "const struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
		.cast = _mSTStructCast_ ## STRUCT, \
	}

#define mSCRIPT_DECLARE_STRUCT(STRUCT) \
	extern const struct mScriptType mSTStruct_ ## STRUCT; \
	extern const struct mScriptType mSTStructConst_ ## STRUCT;

#define mSCRIPT_DEFINE_STRUCT(STRUCT) \
	const struct mScriptType mSTStruct_ ## STRUCT; \
	const struct mScriptType mSTStructConst_ ## STRUCT; \
	static struct mScriptTypeClass _mSTStructDetails_ ## STRUCT = { \
		.init = false, \
		.details = (const struct mScriptClassInitDetails[]) {

#define mSCRIPT_DEFINE_DOCSTRING(DOCSTRING) { \
	.type = mSCRIPT_CLASS_INIT_DOCSTRING, \
	.info = { \
		.comment = DOCSTRING \
	} \
},

#define mSCRIPT_DEFINE_STRUCT_MEMBER(STRUCT, TYPE, NAME) { \
	.type = mSCRIPT_CLASS_INIT_INSTANCE_MEMBER, \
	.info = { \
		.member = { \
			.name = #NAME, \
			.type = _mAPPLY(mSCRIPT_TYPE_MS_ ## TYPE), \
			.offset = offsetof(struct STRUCT, NAME) \
		} \
	} \
},

#define mSCRIPT_DEFINE_STATIC_MEMBER(TYPE, NAME) { \
	.type = mSCRIPT_CLASS_INIT_STATIC_MEMBER, \
	.info = { \
		.member = { \
			.name = #NAME, \
			.type = _mAPPLY(mSCRIPT_TYPE_MS_ ## TYPE) \
		} \
	}, \
},

#define _mSCRIPT_STRUCT_METHOD_POP(TYPE, S, NPARAMS, ...) \
	_mDEFER(_mDEFER(_mCAT(mSCRIPT_POP_, _mSUCC ## NPARAMS)) (&frame->arguments, _mCOMMA_ ## NPARAMS(S(TYPE), __VA_ARGS__))); \
	if (mScriptListSize(&frame->arguments)) { \
		return false; \
	}

#define _mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, NRET, RETURN, NPARAMS, ...) \
	static bool _mSTStructBinding_ ## TYPE ## _ ## NAME(struct mScriptFrame* frame, void* ctx); \
	static const struct mScriptFunction _mSTStructBindingFunction_ ## TYPE ## _ ## NAME = { \
		.call = &_mSTStructBinding_ ## TYPE ## _ ## NAME \
	}; \
	\
	static void _mSTStructBindingAlloc_ ## TYPE ## _ ## NAME(struct mScriptValue* val) { \
		val->value.copaque = &_mSTStructBindingFunction_ ## TYPE ## _ ## NAME; \
	}\
	static const struct mScriptType _mSTStructBindingType_ ## TYPE ## _ ## NAME = { \
		.base = mSCRIPT_TYPE_FUNCTION, \
		.name = "struct::" #TYPE "." #NAME, \
		.alloc = _mSTStructBindingAlloc_ ## TYPE ## _ ## NAME, \
		.details = { \
			.function = { \
				.parameters = { \
					.count = _mSUCC ## NPARAMS, \
					.entries = { _mAPPLY(mSCRIPT_TYPE_MS_ ## S(TYPE)), mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_MS_, __VA_ARGS__) } \
				}, \
				.returnType = { \
					.count = NRET, \
					.entries = { RETURN } \
				}, \
			}, \
		} \
	};

#define mSCRIPT_DECLARE_STRUCT_D_METHOD(TYPE, RETURN, NAME, NPARAMS, ...) \
	typedef _mAPPLY(mSCRIPT_TYPE_C_ ## RETURN) (*_mSTStructFunctionType_ ## TYPE ## _ ## NAME)(_mAPPLY(_mCOMMA_ ## NPARAMS(struct TYPE* , mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_C_, __VA_ARGS__)))); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS, __VA_ARGS__) \
	\
	static bool _mSTStructBinding_ ## TYPE ## _ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mSCRIPT_STRUCT_METHOD_POP(TYPE, S, NPARAMS, __VA_ARGS__); \
		_mSCRIPT_CALL(RETURN, p0->NAME, _mSUCC ## NPARAMS); \
		return true; \
	} \

#define mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(TYPE, NAME, NPARAMS, ...) \
	typedef void (*_mSTStructFunctionType_ ## TYPE ## _ ## NAME)(_mAPPLY(_mCOMMA_ ## NPARAMS(struct TYPE* , mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_C_, __VA_ARGS__)))); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, 0, , NPARAMS, __VA_ARGS__) \
	\
	static bool _mSTStructBinding_ ## TYPE ## _ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mSCRIPT_STRUCT_METHOD_POP(TYPE, S, NPARAMS, __VA_ARGS__); \
		_mSCRIPT_CALL_VOID(p0->NAME, _mSUCC ## NPARAMS); \
		return true; \
	} \

#define mSCRIPT_DECLARE_STRUCT_CD_METHOD(TYPE, RETURN, NAME, NPARAMS, ...) \
	typedef _mAPPLY(mSCRIPT_TYPE_C_ ## RETURN) (*_mSTStructFunctionType_ ## TYPE ## _ ## NAME)(_mAPPLY(_mCOMMA_ ## NPARAMS(struct TYPE* , mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_C_, __VA_ARGS__)))); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, CS, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS, __VA_ARGS__) \
	\
	static bool _mSTStructBinding_ ## TYPE ## _ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mSCRIPT_STRUCT_METHOD_POP(TYPE, CS, NPARAMS, __VA_ARGS__); \
		_mSCRIPT_CALL(RETURN, p0->NAME, _mSUCC ## NPARAMS); \
		return true; \
	} \

#define mSCRIPT_DECLARE_STRUCT_VOID_CD_METHOD(TYPE, NAME, NPARAMS, ...) \
	typedef void (*_mSTStructFunctionType_ ## TYPE ## _ ## NAME)(_mAPPLY(_mCOMMA_ ## NPARAMS(struct TYPE* , mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_C_, __VA_ARGS__)))); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, CS, 0, , NPARAMS, __VA_ARGS__) \
	\
	static bool _mSTStructBinding_ ## TYPE ## _ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mSCRIPT_STRUCT_METHOD_POP(TYPE, CS, NPARAMS, __VA_ARGS__); \
		_mSCRIPT_CALL_VOID(p0->NAME, _mSUCC ## NPARAMS); \
		return true; \
	} \

#define mSCRIPT_DEFINE_STRUCT_METHOD(TYPE, NAME) { \
	.type = mSCRIPT_CLASS_INIT_INSTANCE_MEMBER, \
	.info = { \
		.member = { \
			.name = #NAME, \
			.type = &_mSTStructBindingType_ ## TYPE ## _ ## NAME \
		} \
	}, \
},

#define mSCRIPT_DEFINE_END { .type = mSCRIPT_CLASS_INIT_END } } }

#define _mSCRIPT_BIND_FUNCTION(NAME, NRET, RETURN, NPARAMS, ...) \
	static const struct mScriptType _type_ ## NAME = { \
		.base = mSCRIPT_TYPE_FUNCTION, \
		.details = { \
			.function = { \
				.parameters = { \
					.count = NPARAMS, \
					.entries = { _mAPPLY(mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_MS_, __VA_ARGS__)) } \
				}, \
				.returnType = { \
					.count = NRET, \
					.entries = { RETURN } \
				}, \
			}, \
		} \
	}; \
	static struct mScriptFunction _function_ ## NAME = { \
		.call = _binding_ ## NAME \
	}; \
	const struct mScriptValue NAME = { \
		.type = &_type_ ## NAME, \
		.refs = mSCRIPT_VALUE_UNREF, \
		.value = { \
			.copaque = &_function_ ## NAME \
		} \
	}

#define mSCRIPT_BIND_FUNCTION(NAME, RETURN, FUNCTION, NPARAMS, ...) \
	static bool _binding_ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		mSCRIPT_POP_ ## NPARAMS(&frame->arguments, __VA_ARGS__); \
		if (mScriptListSize(&frame->arguments)) { \
			return false; \
		} \
		_mSCRIPT_CALL(RETURN, FUNCTION, NPARAMS); \
		return true; \
	} \
	_mSCRIPT_BIND_FUNCTION(NAME, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS, __VA_ARGS__)

#define mSCRIPT_BIND_VOID_FUNCTION(NAME, FUNCTION, NPARAMS, ...) \
	static bool _binding_ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		mSCRIPT_POP_ ## NPARAMS(&frame->arguments, __VA_ARGS__); \
		if (mScriptListSize(&frame->arguments)) { \
			return false; \
		} \
		_mSCRIPT_CALL_VOID(FUNCTION, NPARAMS); \
		return true; \
	} \
	_mSCRIPT_BIND_FUNCTION(NAME, 0, , NPARAMS, __VA_ARGS__)

#define mSCRIPT_MAKE(TYPE, FIELD, VALUE) (struct mScriptValue) { \
		.type = (TYPE), \
		.refs = mSCRIPT_VALUE_UNREF, \
		.value = { \
			.FIELD = (VALUE) \
		}, \
	} \

#define mSCRIPT_MAKE_S8(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_S8, s32, VALUE)
#define mSCRIPT_MAKE_U8(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_U8, u32, VALUE)
#define mSCRIPT_MAKE_S16(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_S16, s32, VALUE)
#define mSCRIPT_MAKE_U16(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_U16, u32, VALUE)
#define mSCRIPT_MAKE_S32(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_S32, s32, VALUE)
#define mSCRIPT_MAKE_U32(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_U32, u32, VALUE)
#define mSCRIPT_MAKE_F32(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_F32, f32, VALUE)
#define mSCRIPT_MAKE_S64(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_S64, s64, VALUE)
#define mSCRIPT_MAKE_U64(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_U64, u64, VALUE)
#define mSCRIPT_MAKE_F64(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_F64, f64, VALUE)
#define mSCRIPT_MAKE_CHARP(VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_CHARP, opaque, VALUE)
#define mSCRIPT_MAKE_S(STRUCT, VALUE) mSCRIPT_MAKE(mSCRIPT_TYPE_MS_S(STRUCT), opaque, VALUE)

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
	mSCRIPT_TYPE_WRAPPER
};

enum mScriptClassInitType {
	mSCRIPT_CLASS_INIT_END = 0,
	mSCRIPT_CLASS_INIT_DOCSTRING,
	mSCRIPT_CLASS_INIT_INSTANCE_MEMBER,
	mSCRIPT_CLASS_INIT_STATIC_MEMBER,
	mSCRIPT_CLASS_INIT_INHERIT,
};

struct Table;
struct mScriptType;
extern const struct mScriptType mSTVoid;
extern const struct mScriptType mSTSInt8;
extern const struct mScriptType mSTUInt8;
extern const struct mScriptType mSTSInt16;
extern const struct mScriptType mSTUInt16;
extern const struct mScriptType mSTSInt32;
extern const struct mScriptType mSTUInt32;
extern const struct mScriptType mSTFloat32;
extern const struct mScriptType mSTSInt64;
extern const struct mScriptType mSTUInt64;
extern const struct mScriptType mSTFloat64;
extern const struct mScriptType mSTString;
extern const struct mScriptType mSTCharPtr;
extern const struct mScriptType mSTTable;
extern const struct mScriptType mSTWrapper;

struct mScriptTypeTuple {
	size_t count;
	const struct mScriptType* entries[mSCRIPT_PARAMS_MAX];
	bool variable;
};

struct mScriptTypeFunction {
	struct mScriptTypeTuple parameters;
	struct mScriptTypeTuple returnType;
	// TODO: kwargs, defaults
};

struct mScriptClassMember {
	const char* name;
	const char* docstring;
	const struct mScriptType* type;
	size_t offset;
};

struct mScriptClassInitDetails {
	enum mScriptClassInitType type;
	union {
		const char* comment;
		const struct mScriptTypeClass* parent;
		struct mScriptClassMember member;
	} info;
};

struct mScriptTypeClass {
	bool init;
	const struct mScriptClassInitDetails* details;
	struct Table staticMembers;
	struct Table instanceMembers;
};

struct mScriptValue;
struct mScriptType {
	int base;
	size_t size;
	const char* name;
	union {
		struct mScriptTypeTuple tuple;
		struct mScriptTypeFunction function;
		struct mScriptTypeClass* cls;
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
		int64_t s64;
		uint64_t u64;
		double f64;
		void* opaque;
		const void* copaque;
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
	bool (*call)(struct mScriptFrame*, void* context);
	void* context;
};

struct mScriptValue* mScriptValueAlloc(const struct mScriptType* type);
void mScriptValueRef(struct mScriptValue* val);
void mScriptValueDeref(struct mScriptValue* val);

void mScriptValueWrap(struct mScriptValue* val, struct mScriptValue* out);
struct mScriptValue* mScriptValueUnwrap(struct mScriptValue* val);

struct mScriptValue* mScriptStringCreateFromUTF8(const char* string);

bool mScriptTableInsert(struct mScriptValue* table, struct mScriptValue* key, struct mScriptValue* value);
bool mScriptTableRemove(struct mScriptValue* table, struct mScriptValue* key);
struct mScriptValue* mScriptTableLookup(struct mScriptValue* table, struct mScriptValue* key);

void mScriptFrameInit(struct mScriptFrame* frame);
void mScriptFrameDeinit(struct mScriptFrame* frame);

void mScriptClassInit(struct mScriptTypeClass* cls);
void mScriptClassDeinit(struct mScriptTypeClass* cls);

bool mScriptObjectGet(struct mScriptValue* obj, const char* member, struct mScriptValue*);

bool mScriptPopS32(struct mScriptList* list, int32_t* out);
bool mScriptPopU32(struct mScriptList* list, uint32_t* out);
bool mScriptPopF32(struct mScriptList* list, float* out);
bool mScriptPopS64(struct mScriptList* list, int64_t* out);
bool mScriptPopU64(struct mScriptList* list, uint64_t* out);
bool mScriptPopF64(struct mScriptList* list, double* out);
bool mScriptPopPointer(struct mScriptList* list, void** out);

bool mScriptCast(const struct mScriptType* type, const struct mScriptValue* input, struct mScriptValue* output);
bool mScriptCoerceFrame(const struct mScriptTypeTuple* types, struct mScriptList* frame);

#endif
