/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_MACROS_H
#define M_SCRIPT_MACROS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#define mSCRIPT_POP(STACK, TYPE, NAME) \
	mSCRIPT_TYPE_C_ ## TYPE NAME; \
	do { \
		struct mScriptValue* _val = mScriptListGetPointer(STACK, mScriptListSize(STACK) - 1); \
		bool deref = true; \
		if (!(mSCRIPT_TYPE_CMP(TYPE, _val->type))) { \
			if (_val->type->base == mSCRIPT_TYPE_WRAPPER) { \
				_val = mScriptValueUnwrap(_val); \
				deref = false; \
				if (!(mSCRIPT_TYPE_CMP(TYPE, _val->type))) { \
					return false; \
				} \
			} else { \
				return false; \
			} \
		} \
		NAME = _val->value.mSCRIPT_TYPE_FIELD_ ## TYPE; \
		if (deref) { \
			mScriptValueDeref(_val); \
		} \
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
		_val->type = mSCRIPT_TYPE_MS_ ## TYPE; \
		_val->refs = mSCRIPT_VALUE_UNREF; \
		_val->flags = 0; \
		_val->value.mSCRIPT_TYPE_FIELD_ ## TYPE = NAME; \
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
#define mSCRIPT_PREFIX_N(N) mSCRIPT_PREFIX_ ## N

#define _mSCRIPT_FIELD_NAME(V) (V)->name

#define _mSCRIPT_CALL_VOID(FUNCTION, NPARAMS) FUNCTION(_mCAT(mSCRIPT_ARG_NAMES_, NPARAMS))
#define _mSCRIPT_CALL(RETURN, FUNCTION, NPARAMS) \
	mSCRIPT_TYPE_C_ ## RETURN out = FUNCTION(_mCAT(mSCRIPT_ARG_NAMES_, NPARAMS)); \
	mSCRIPT_PUSH(&frame->returnValues, RETURN, out)

#define mSCRIPT_DECLARE_STRUCT(STRUCT) \
	extern const struct mScriptType mSTStruct_ ## STRUCT; \
	extern const struct mScriptType mSTStructConst_ ## STRUCT; \
	extern const struct mScriptType mSTStructPtr_ ## STRUCT; \
	extern const struct mScriptType mSTStructPtrConst_ ## STRUCT; \
	extern const struct mScriptType mSTWrapper_ ## STRUCT; \
	extern const struct mScriptType mSTWrapperConst_ ## STRUCT;

#define mSCRIPT_DEFINE_STRUCT(STRUCT) \
	const struct mScriptType mSTStruct_ ## STRUCT; \
	const struct mScriptType mSTStructConst_ ## STRUCT; \
	const struct mScriptType mSTStructPtr_ ## STRUCT; \
	const struct mScriptType mSTStructPtrConst_ ## STRUCT; \
	static struct mScriptTypeClass _mSTStructDetails_ ## STRUCT; \
	static bool _mSTStructPtrCast_ ## STRUCT(const struct mScriptValue* input, const struct mScriptType* type, struct mScriptValue* output) { \
		if (input->type == type || (input->type->constType == type)) { \
			output->type = type; \
			output->value.opaque = input->value.opaque; \
			return true; \
		} \
		if (input->type != &mSTStructPtr_ ## STRUCT && input->type != &mSTStructPtrConst_ ## STRUCT) { \
			return false; \
		} \
		if (type == &mSTStructConst_ ## STRUCT || (!input->type->isConst && type == &mSTStruct_ ## STRUCT)) { \
			output->type = type; \
			output->value.opaque = *(void**) input->value.opaque; \
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
		.free = mScriptObjectFree, \
		.cast = mScriptObjectCast, \
		.constType = &mSTStructConst_ ## STRUCT, \
	}; \
	const struct mScriptType mSTStructConst_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_OBJECT, \
		.isConst = true, \
		.details = { \
			.cls = &_mSTStructDetails_ ## STRUCT \
		}, \
		.size = sizeof(struct STRUCT), \
		.name = "const struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
		.cast = mScriptObjectCast, \
	}; \
	const struct mScriptType mSTStructPtr_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_OPAQUE, \
		.details = { \
			.type = &mSTStruct_ ## STRUCT \
		}, \
		.size = sizeof(void*), \
		.name = "ptr struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
		.cast = _mSTStructPtrCast_ ## STRUCT, \
		.constType = &mSTStructPtrConst_ ## STRUCT, \
	}; \
	const struct mScriptType mSTStructPtrConst_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_OPAQUE, \
		.details = { \
			.type = &mSTStructConst_ ## STRUCT \
		}, \
		.isConst = true, \
		.size = sizeof(void*), \
		.name = "ptr const struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
		.cast = _mSTStructPtrCast_ ## STRUCT, \
	}; \
	const struct mScriptType mSTWrapper_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_WRAPPER, \
		.details = { \
			.type = &mSTStruct_ ## STRUCT \
		}, \
		.size = sizeof(struct mScriptValue), \
		.name = "wrapper struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
	}; \
	const struct mScriptType mSTWrapperConst_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_WRAPPER, \
		.details = { \
			.type = &mSTStructConst_ ## STRUCT \
		}, \
		.size = sizeof(struct mScriptValue), \
		.name = "wrapper const struct::" #STRUCT, \
		.alloc = NULL, \
		.free = NULL, \
	}; \
	static struct mScriptTypeClass _mSTStructDetails_ ## STRUCT = { \
		.init = false, \
		.details = (const struct mScriptClassInitDetails[]) {

#define mSCRIPT_DECLARE_DOC_STRUCT(SCOPE, STRUCT) \
	static const struct mScriptType mSTStruct_doc_ ## STRUCT;

#define mSCRIPT_DEFINE_DOC_STRUCT(SCOPE, STRUCT) \
	static struct mScriptTypeClass _mSTStructDetails_doc_ ## STRUCT; \
	static const struct mScriptType mSTStruct_doc_ ## STRUCT = { \
		.base = mSCRIPT_TYPE_OBJECT, \
		.details = { \
			.cls = &_mSTStructDetails_doc_ ## STRUCT \
		}, \
		.size = 0, \
		.name = SCOPE "::struct::" #STRUCT, \
	}; \
	static struct mScriptTypeClass _mSTStructDetails_doc_ ## STRUCT = { \
		.init = false, \
		.details = (const struct mScriptClassInitDetails[]) {

#define mSCRIPT_DEFINE_DOCSTRING(DOCSTRING) { \
	.type = mSCRIPT_CLASS_INIT_DOCSTRING, \
	.info = { \
		.comment = DOCSTRING \
	} \
},
#define mSCRIPT_DEFINE_CLASS_DOCSTRING(DOCSTRING) { \
	.type = mSCRIPT_CLASS_INIT_CLASS_DOCSTRING, \
	.info = { \
		.comment = DOCSTRING \
	} \
},

#define _mSCRIPT_DEFINE_STRUCT_MEMBER(STRUCT, TYPE, EXPORTED_NAME, NAME, RO) { \
	.type = mSCRIPT_CLASS_INIT_INSTANCE_MEMBER, \
	.info = { \
		.member = { \
			.name = #EXPORTED_NAME, \
			.type = mSCRIPT_TYPE_MS_ ## TYPE, \
			.offset = offsetof(struct STRUCT, NAME), \
			.readonly = RO \
		} \
	} \
},

#define mSCRIPT_DEFINE_STRUCT_MEMBER_NAMED(STRUCT, TYPE, EXPORTED_NAME, NAME) \
	_mSCRIPT_DEFINE_STRUCT_MEMBER(STRUCT, TYPE, EXPORTED_NAME, NAME, false)

#define mSCRIPT_DEFINE_STRUCT_CONST_MEMBER_NAMED(STRUCT, TYPE, EXPORTED_NAME, NAME) \
	_mSCRIPT_DEFINE_STRUCT_MEMBER(STRUCT, TYPE, EXPORTED_NAME, NAME, true)

#define mSCRIPT_DEFINE_STRUCT_MEMBER(STRUCT, TYPE, NAME) \
	mSCRIPT_DEFINE_STRUCT_MEMBER_NAMED(STRUCT, TYPE, NAME, NAME)

#define mSCRIPT_DEFINE_STRUCT_CONST_MEMBER(STRUCT, TYPE, NAME) \
	mSCRIPT_DEFINE_STRUCT_CONST_MEMBER_NAMED(STRUCT, TYPE, NAME, NAME)

#define mSCRIPT_DEFINE_INHERIT(PARENT) { \
	.type = mSCRIPT_CLASS_INIT_INHERIT, \
	.info = { \
		.parent = mSCRIPT_TYPE_MS_S(PARENT) \
	} \
},

#define mSCRIPT_DEFINE_INTERNAL { \
	.type = mSCRIPT_CLASS_INIT_INTERNAL \
},

#define _mSCRIPT_STRUCT_METHOD_POP(TYPE, S, NPARAMS, ...) \
	_mCALL(_mCAT(mSCRIPT_POP_, _mSUCC_ ## NPARAMS), &frame->arguments, _mCOMMA_ ## NPARAMS(S(TYPE), __VA_ARGS__)); \
	if (mScriptListSize(&frame->arguments)) { \
		return false; \
	}

#define _mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, NRET, RETURN, NPARAMS, DEFAULTS, ...) \
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
					.count = _mSUCC_ ## NPARAMS, \
					.entries = { mSCRIPT_TYPE_MS_ ## S(TYPE), _mCALL(mSCRIPT_PREFIX_ ## NPARAMS, mSCRIPT_TYPE_MS_, _mEVEN_ ## NPARAMS(__VA_ARGS__)) }, \
					.names = { "this", _mCALL(_mCALL_ ## NPARAMS, _mSTRINGIFY, _mODD_ ## NPARAMS(__VA_ARGS__)) }, \
					.defaults = DEFAULTS, \
				}, \
				.returnType = { \
					.count = NRET, \
					.entries = { RETURN } \
				}, \
			}, \
		} \
	};

#define _mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, RETURN, NAME, CONST, NPARAMS, ...) \
	typedef RETURN (*_mSTStructFunctionType_ ## TYPE ## _ ## NAME)(_mCOMMA_ ## NPARAMS(CONST struct TYPE* , mSCRIPT_PREFIX_ ## NPARAMS(mSCRIPT_TYPE_C_, __VA_ARGS__)))

#define _mSCRIPT_DECLARE_STRUCT_METHOD_BINDING(TYPE, RETURN, NAME, FUNCTION, T, NPARAMS, ...) \
	static bool _mSTStructBinding_ ## TYPE ## _ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mSCRIPT_STRUCT_METHOD_POP(TYPE, T, NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
		_mSCRIPT_CALL(RETURN, FUNCTION, _mSUCC_ ## NPARAMS); \
		return true; \
	}

#define _mSCRIPT_DECLARE_STRUCT_VOID_METHOD_BINDING(TYPE, NAME, FUNCTION, T, NPARAMS, ...) \
	static bool _mSTStructBinding_ ## TYPE ## _ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mSCRIPT_STRUCT_METHOD_POP(TYPE, T, NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
		_mSCRIPT_CALL_VOID(FUNCTION, _mSUCC_ ## NPARAMS); \
		return true; \
	} \

#define mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, RETURN, NAME, FUNCTION, NPARAMS, ...) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, mSCRIPT_TYPE_C_ ## RETURN, NAME, , NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS, NULL, __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_BINDING(TYPE, RETURN, NAME, FUNCTION, S, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TYPE, NAME, FUNCTION, NPARAMS, ...) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, void, NAME, , NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, 0, 0, NPARAMS, NULL, __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_VOID_METHOD_BINDING(TYPE, NAME, FUNCTION, S, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_C_METHOD(TYPE, RETURN, NAME, FUNCTION, NPARAMS, ...) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, mSCRIPT_TYPE_C_ ## RETURN, NAME, const, NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, CS, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS, NULL, __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_BINDING(TYPE, RETURN, NAME, FUNCTION, CS, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_C_METHOD(TYPE, NAME, FUNCTION, NPARAMS, ...) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, void, NAME, const, NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, CS, 0, 0, NPARAMS, NULL, __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_VOID_METHOD_BINDING(TYPE, NAME, FUNCTION, CS, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(TYPE, RETURN, NAME, FUNCTION, NPARAMS, ...) \
	static const struct mScriptValue _mSTStructBindingDefaults_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, mSCRIPT_TYPE_C_ ## RETURN, NAME, , NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS,  _mIDENT(_mSTStructBindingDefaults_ ## TYPE ## _ ## NAME), __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_BINDING(TYPE, RETURN, NAME, FUNCTION, S, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_METHOD_WITH_DEFAULTS(TYPE, NAME, FUNCTION, NPARAMS, ...) \
	static const struct mScriptValue _mSTStructBindingDefaults_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, void, NAME, , NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, S, 0, 0, NPARAMS, _mIDENT(_mSTStructBindingDefaults_ ## TYPE ## _ ## NAME), __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_VOID_METHOD_BINDING(TYPE, NAME, FUNCTION, S, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_C_METHOD_WITH_DEFAULTS(TYPE, RETURN, NAME, FUNCTION, NPARAMS, ...) \
	static const struct mScriptValue _mSTStructBindingDefaults_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, mSCRIPT_TYPE_C_ ## RETURN, NAME, const, NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, CS, 1, mSCRIPT_TYPE_MS_##RETURN, NPARAMS, _mIDENT(_mSTStructBindingDefaults_ ## TYPE ## _ ## NAME), __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_METHOD_BINDING(TYPE, RETURN, NAME, FUNCTION, CS, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_C_METHOD_WITH_DEFAULTS(TYPE, NAME, FUNCTION, NPARAMS, ...) \
	static const struct mScriptValue _mSTStructBindingDefaults_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_DECLARE_STRUCT_METHOD_SIGNATURE(TYPE, void, NAME, const, NPARAMS, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
	_mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, NAME, CS, 0, 0, NPARAMS, _mIDENT(_mSTStructBindingDefaults_ ## TYPE ## _ ## NAME, __VA_ARGS__) \
	_mSCRIPT_DECLARE_STRUCT_VOID_METHOD_BINDING(TYPE, NAME, FUNCTION, CS, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_D_METHOD(TYPE, RETURN, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_METHOD(TYPE, RETURN, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(TYPE, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_VOID_METHOD(TYPE, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_CD_METHOD(TYPE, RETURN, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_C_METHOD(TYPE, RETURN, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_CD_METHOD(TYPE, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_VOID_C_METHOD(TYPE, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_D_METHOD_WITH_DEFAULTS(TYPE, RETURN, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(TYPE, RETURN, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD_WITH_DEFAULTS(TYPE, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_VOID_METHOD_WITH_DEFAULTS(TYPE, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_CD_METHOD_WITH_DEFAULTS(TYPE, RETURN, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_C_METHOD_WITH_DEFAULTS(TYPE, RETURN, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DECLARE_STRUCT_VOID_CD_METHOD_WITH_DEFAULTS(TYPE, NAME, NPARAMS, ...) \
	mSCRIPT_DECLARE_STRUCT_VOID_C_METHOD_WITH_DEFAULTS(TYPE, NAME, p0->NAME, NPARAMS, __VA_ARGS__)

#define _mSCRIPT_DECLARE_DOC_STRUCT_METHOD(SCOPE, TYPE, NAME, S, NRET, RETURN, NPARAMS, DEFAULTS, ...) \
	static const struct mScriptType _mSTStructBindingType_doc_ ## TYPE ## _ ## NAME = { \
		.base = mSCRIPT_TYPE_FUNCTION, \
		.name = SCOPE "::struct::" #TYPE "." #NAME, \
		.details = { \
			.function = { \
				.parameters = { \
					.count = _mSUCC_ ## NPARAMS, \
					.entries = { mSCRIPT_TYPE_MS_DS(TYPE), _mCALL(mSCRIPT_PREFIX_ ## NPARAMS, mSCRIPT_TYPE_MS_, _mEVEN_ ## NPARAMS(__VA_ARGS__)) }, \
					.names = { "this", _mCALL(_mCALL_ ## NPARAMS, _mSTRINGIFY, _mODD_ ## NPARAMS(__VA_ARGS__)) }, \
					.defaults = DEFAULTS, \
				}, \
				.returnType = { \
					.count = NRET, \
					.entries = { RETURN } \
				}, \
			}, \
		} \
	};

#define mSCRIPT_DECLARE_DOC_STRUCT_METHOD(SCOPE, TYPE, RETURN, NAME, NPARAMS, ...) \
	_mSCRIPT_DECLARE_DOC_STRUCT_METHOD(SCOPE, TYPE, NAME, S, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS, NULL, __VA_ARGS__)

#define mSCRIPT_DECLARE_DOC_STRUCT_VOID_METHOD(SCOPE, TYPE, NAME, NPARAMS, ...) \
	_mSCRIPT_DECLARE_DOC_STRUCT_METHOD(SCOPE, TYPE, NAME, S, 0, 0, NPARAMS, NULL, __VA_ARGS__)

#define mSCRIPT_DECLARE_DOC_STRUCT_METHOD_WITH_DEFAULTS(SCOPE, TYPE, RETURN, NAME, NPARAMS, ...) \
	static const struct mScriptValue _mSTStructBindingDefaults_doc_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_DECLARE_DOC_STRUCT_METHOD(SCOPE, TYPE, NAME, S, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS,  _mIDENT(_mSTStructBindingDefaults_doc_ ## TYPE ## _ ## NAME), __VA_ARGS__) \

#define mSCRIPT_DECLARE_DOC_STRUCT_VOID_METHOD_WITH_DEFAULTS(SCOPE, TYPE, NAME, NPARAMS, ...) \
	static const struct mScriptValue _mSTStructBindingDefaults_doc_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_DECLARE_DOC_STRUCT_METHOD(SCOPE, TYPE, NAME, S, 0, 0, NPARAMS, _mIDENT(_mSTStructBindingDefaults_doc_ ## TYPE ## _ ## NAME), __VA_ARGS__) \

#define mSCRIPT_DEFINE_FUNCTION_BINDING_DEFAULTS(NAME) \
	static const struct mScriptValue _bindingDefaults_ ## NAME[mSCRIPT_PARAMS_MAX] = {

#define mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(TYPE, NAME) \
	static const struct mScriptValue _mSTStructBindingDefaults_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX] = { \
		mSCRIPT_NO_DEFAULT,

#define mSCRIPT_DEFINE_DOC_STRUCT_BINDING_DEFAULTS(SCOPE, TYPE, NAME) \
	static const struct mScriptValue _mSTStructBindingDefaults_doc_ ## TYPE ## _ ## NAME[mSCRIPT_PARAMS_MAX] = { \
		mSCRIPT_NO_DEFAULT,

#define mSCRIPT_DEFINE_DEFAULTS_END }

#define _mSCRIPT_DEFINE_STRUCT_BINDING(INIT_TYPE, TYPE, EXPORTED_NAME, NAME) { \
	.type = mSCRIPT_CLASS_INIT_ ## INIT_TYPE, \
	.info = { \
		.member = { \
			.name = #EXPORTED_NAME, \
			.type = &_mSTStructBindingType_ ## TYPE ## _ ## NAME \
		} \
	}, \
},

#define mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(TYPE, EXPORTED_NAME, NAME) \
	_mSCRIPT_DEFINE_STRUCT_BINDING(INSTANCE_MEMBER, TYPE, EXPORTED_NAME, NAME)

#define mSCRIPT_DEFINE_STRUCT_METHOD(TYPE, NAME) mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(TYPE, NAME, NAME)

#define mSCRIPT_DEFINE_STRUCT_INIT(TYPE) _mSCRIPT_DEFINE_STRUCT_BINDING(INIT, TYPE, _init, _init)
#define mSCRIPT_DEFINE_STRUCT_INIT_NAMED(TYPE, NAME) _mSCRIPT_DEFINE_STRUCT_BINDING(INIT, TYPE, _init, NAME)
#define mSCRIPT_DEFINE_STRUCT_DEINIT(TYPE) _mSCRIPT_DEFINE_STRUCT_BINDING(DEINIT, TYPE, _deinit, _deinit)
#define mSCRIPT_DEFINE_STRUCT_DEINIT_NAMED(TYPE, NAME) _mSCRIPT_DEFINE_STRUCT_BINDING(DEINIT, TYPE, _deinit, NAME)
#define mSCRIPT_DEFINE_STRUCT_DEFAULT_GET(TYPE) _mSCRIPT_DEFINE_STRUCT_BINDING(GET, TYPE, _get, _get)
#define mSCRIPT_DEFINE_STRUCT_DEFAULT_SET(TYPE, SETTER) _mSCRIPT_DEFINE_STRUCT_BINDING(SET, TYPE, SETTER, SETTER)

#define mSCRIPT_DEFINE_DOC_STRUCT_METHOD(SCOPE, TYPE, NAME) mSCRIPT_DEFINE_STRUCT_METHOD_NAMED(doc_ ## TYPE, NAME, NAME)

#define mSCRIPT_DEFINE_STRUCT_CAST_TO_MEMBER(TYPE, CAST_TYPE, MEMBER) { \
	.type = mSCRIPT_CLASS_INIT_CAST_TO_MEMBER, \
	.info = { \
		.castMember = { \
			.type = mSCRIPT_TYPE_MS_ ## CAST_TYPE, \
			.member = #MEMBER \
		} \
	}, \
},

#define mSCRIPT_DEFINE_END { .type = mSCRIPT_CLASS_INIT_END } } }

#define _mSCRIPT_BIND_FUNCTION(NAME, NRET, RETURN, DEFAULTS, NPARAMS, ...) \
	static struct mScriptFunction _function_ ## NAME = { \
		.call = _binding_ ## NAME \
	}; \
	static void _alloc_ ## NAME(struct mScriptValue* val) { \
		val->value.copaque = &_function_ ## NAME; \
	} \
	static const struct mScriptType _type_ ## NAME = { \
		.base = mSCRIPT_TYPE_FUNCTION, \
		.name = "function::" #NAME, \
		.alloc = _alloc_ ## NAME, \
		.details = { \
			.function = { \
				.parameters = { \
					.count = NPARAMS, \
					.entries = { _mCALL(_mIF0_ ## NPARAMS, 0) _mCALL(mSCRIPT_PREFIX_ ## NPARAMS, mSCRIPT_TYPE_MS_, _mEVEN_ ## NPARAMS(__VA_ARGS__)) }, \
					.names = { _mCALL(_mIF0_ ## NPARAMS, 0) _mCALL(_mCALL_ ## NPARAMS, _mSTRINGIFY, _mODD_ ## NPARAMS(__VA_ARGS__)) }, \
					.defaults = DEFAULTS, \
				}, \
				.returnType = { \
					.count = NRET, \
					.entries = { RETURN } \
				}, \
			}, \
		} \
	}; \
	const struct mScriptValue NAME = { \
		.type = &_type_ ## NAME, \
		.refs = mSCRIPT_VALUE_UNREF, \
		.value = { \
			.copaque = &_function_ ## NAME \
		} \
	}

#define _mSCRIPT_BIND_N_FUNCTION(NAME, RETURN, FUNCTION, DEFAULTS, NPARAMS, ...) \
	static bool _binding_ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mCALL(mSCRIPT_POP_ ## NPARAMS, &frame->arguments, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
		if (mScriptListSize(&frame->arguments)) { \
			return false; \
		} \
		_mSCRIPT_CALL(RETURN, FUNCTION, NPARAMS); \
		return true; \
	} \
	_mSCRIPT_BIND_FUNCTION(NAME, 1, mSCRIPT_TYPE_MS_ ## RETURN, DEFAULTS, NPARAMS, __VA_ARGS__)

#define _mSCRIPT_BIND_VOID_FUNCTION(NAME, FUNCTION, DEFAULTS, NPARAMS, ...) \
	static bool _binding_ ## NAME(struct mScriptFrame* frame, void* ctx) { \
		UNUSED(ctx); \
		_mCALL(mSCRIPT_POP_ ## NPARAMS, &frame->arguments, _mEVEN_ ## NPARAMS(__VA_ARGS__)); \
		if (mScriptListSize(&frame->arguments)) { \
			return false; \
		} \
		_mSCRIPT_CALL_VOID(FUNCTION, NPARAMS); \
		return true; \
	} \
	_mSCRIPT_BIND_FUNCTION(NAME, 0, 0, NULL, NPARAMS, __VA_ARGS__)

#define mSCRIPT_BIND_FUNCTION(NAME, RETURN, FUNCTION, NPARAMS, ...) \
	_mSCRIPT_BIND_N_FUNCTION(NAME, RETURN, FUNCTION, NULL, NPARAMS, __VA_ARGS__)

#define mSCRIPT_BIND_VOID_FUNCTION(NAME, FUNCTION, NPARAMS, ...) \
	_mSCRIPT_BIND_VOID_FUNCTION(NAME, FUNCTION, NULL, NPARAMS, __VA_ARGS__)

#define mSCRIPT_BIND_FUNCTION_WITH_DEFAULTS(NAME, RETURN, FUNCTION, NPARAMS, ...) \
	static const struct mScriptValue _bindingDefaults_ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_BIND_N_FUNCTION(NAME, RETURN, FUNCTION, _mIDENT(_bindingDefaults_ ## NAME), NPARAMS, __VA_ARGS__)

#define mSCRIPT_BIND_VOID_FUNCTION_WITH_DEFAULTS(NAME, FUNCTION, NPARAMS, ...) \
	static const struct mScriptValue _bindingDefaults_ ## _ ## NAME[mSCRIPT_PARAMS_MAX]; \
	_mSCRIPT_BIND_VOID_FUNCTION(NAME, FUNCTION, _mIDENT(_bindingDefaults_ ## NAME), NPARAMS, __VA_ARGS__)

#define _mSCRIPT_DEFINE_DOC_FUNCTION(SCOPE, NAME, NRET, RETURN, NPARAMS, ...) \
	static const struct mScriptType _mScriptDocType_ ## NAME = { \
		.base = mSCRIPT_TYPE_FUNCTION, \
		.name = SCOPE "::function::" #NAME, \
		.alloc = NULL, \
		.details = { \
			.function = { \
				.parameters = { \
					.count = NPARAMS, \
					.entries = { _mCALL(_mIF0_ ## NPARAMS, 0) _mCALL(mSCRIPT_PREFIX_ ## NPARAMS, mSCRIPT_TYPE_MS_, _mEVEN_ ## NPARAMS(__VA_ARGS__)) }, \
					.names = { _mCALL(_mIF0_ ## NPARAMS, 0) _mCALL(_mCALL_ ## NPARAMS, _mSTRINGIFY, _mODD_ ## NPARAMS(__VA_ARGS__)) }, \
				}, \
				.returnType = { \
					.count = NRET, \
					.entries = { RETURN } \
				}, \
			}, \
		} \
	}; \
	const struct mScriptValue _mScriptDoc_ ## NAME = { \
		.type = &_mScriptDocType_ ## NAME, \
		.refs = mSCRIPT_VALUE_UNREF, \
		.value = { \
			.copaque = NULL \
		} \
	}

#define mSCRIPT_DEFINE_DOC_FUNCTION(SCOPE, NAME, RETURN, NPARAMS, ...) \
	_mSCRIPT_DEFINE_DOC_FUNCTION(SCOPE, NAME, 1, mSCRIPT_TYPE_MS_ ## RETURN, NPARAMS, __VA_ARGS__)

#define mSCRIPT_DEFINE_DOC_VOID_FUNCTION(SCOPE, NAME, NPARAMS, ...) \
	_mSCRIPT_DEFINE_DOC_FUNCTION(SCOPE, NAME, 0, 0, NPARAMS, __VA_ARGS__)

#define mSCRIPT_MAKE(TYPE, VALUE) (struct mScriptValue) { \
		.type = (mSCRIPT_TYPE_MS_ ## TYPE), \
		.refs = mSCRIPT_VALUE_UNREF, \
		.value = { \
			.mSCRIPT_TYPE_FIELD_ ## TYPE = (VALUE) \
		}, \
	} \

#define mSCRIPT_VAL(TYPE, VALUE) { \
		.type = (mSCRIPT_TYPE_MS_ ## TYPE), \
		.refs = mSCRIPT_VALUE_UNREF, \
		.value = { \
			.mSCRIPT_TYPE_FIELD_ ## TYPE = (VALUE) \
		}, \
	} \

#define mSCRIPT_MAKE_S8(VALUE) mSCRIPT_MAKE(S8, VALUE)
#define mSCRIPT_MAKE_U8(VALUE) mSCRIPT_MAKE(U8, VALUE)
#define mSCRIPT_MAKE_S16(VALUE) mSCRIPT_MAKE(S16, VALUE)
#define mSCRIPT_MAKE_U16(VALUE) mSCRIPT_MAKE(U16, VALUE)
#define mSCRIPT_MAKE_S32(VALUE) mSCRIPT_MAKE(S32, VALUE)
#define mSCRIPT_MAKE_U32(VALUE) mSCRIPT_MAKE(U32, VALUE)
#define mSCRIPT_MAKE_F32(VALUE) mSCRIPT_MAKE(F32, VALUE)
#define mSCRIPT_MAKE_S64(VALUE) mSCRIPT_MAKE(S64, VALUE)
#define mSCRIPT_MAKE_U64(VALUE) mSCRIPT_MAKE(U64, VALUE)
#define mSCRIPT_MAKE_F64(VALUE) mSCRIPT_MAKE(F64, VALUE)
#define mSCRIPT_MAKE_BOOL(VALUE) mSCRIPT_MAKE(BOOL, VALUE)
#define mSCRIPT_MAKE_CHARP(VALUE) mSCRIPT_MAKE(CHARP, VALUE)
#define mSCRIPT_MAKE_S(STRUCT, VALUE) mSCRIPT_MAKE(S(STRUCT), VALUE)
#define mSCRIPT_MAKE_CS(STRUCT, VALUE) mSCRIPT_MAKE(CS(STRUCT), VALUE)

#define mSCRIPT_S8(VALUE) mSCRIPT_VAL(S8, VALUE)
#define mSCRIPT_U8(VALUE) mSCRIPT_VAL(U8, VALUE)
#define mSCRIPT_S16(VALUE) mSCRIPT_VAL(S16, VALUE)
#define mSCRIPT_U16(VALUE) mSCRIPT_VAL(U16, VALUE)
#define mSCRIPT_S32(VALUE) mSCRIPT_VAL(S32, VALUE)
#define mSCRIPT_U32(VALUE) mSCRIPT_VAL(U32, VALUE)
#define mSCRIPT_F32(VALUE) mSCRIPT_VAL(F32, VALUE)
#define mSCRIPT_S64(VALUE) mSCRIPT_VAL(S64, VALUE)
#define mSCRIPT_U64(VALUE) mSCRIPT_VAL(U64, VALUE)
#define mSCRIPT_F64(VALUE) mSCRIPT_VAL(F64, VALUE)
#define mSCRIPT_CHARP(VALUE) mSCRIPT_VAL(CHARP, VALUE)
#define mSCRIPT_S(STRUCT, VALUE) mSCRIPT_VAL(S(STRUCT), VALUE)
#define mSCRIPT_CS(STRUCT, VALUE) mSCRIPT_VAL(CS(STRUCT), VALUE)

#define mSCRIPT_NO_DEFAULT { \
	.type = NULL, \
	.refs = mSCRIPT_VALUE_UNREF, \
	.value = {0} \
}

CXX_GUARD_END

#endif
