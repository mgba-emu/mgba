/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/script/lua.h>

#include <mgba/script/macros.h>
#include <mgba-util/string.h>

#include <lualib.h>
#include <lauxlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_KEY_SIZE 128

static struct mScriptEngineContext* _luaCreate(struct mScriptEngine2*, struct mScriptContext*);

static void _luaDestroy(struct mScriptEngineContext*);
static bool _luaIsScript(struct mScriptEngineContext*, const char*, struct VFile*);
static struct mScriptValue* _luaGetGlobal(struct mScriptEngineContext*, const char* name);
static bool _luaSetGlobal(struct mScriptEngineContext*, const char* name, struct mScriptValue*);
static bool _luaLoad(struct mScriptEngineContext*, const char*, struct VFile*);
static bool _luaRun(struct mScriptEngineContext*);
static const char* _luaGetError(struct mScriptEngineContext*);

static bool _luaCall(struct mScriptFrame*, void* context);

struct mScriptEngineContextLua;
static bool _luaPushFrame(struct mScriptEngineContextLua*, struct mScriptList*, bool internal);
static bool _luaPopFrame(struct mScriptEngineContextLua*, struct mScriptList*);
static bool _luaInvoke(struct mScriptEngineContextLua*, struct mScriptFrame*);

static struct mScriptValue* _luaCoerce(struct mScriptEngineContextLua* luaContext, bool pop);
static bool _luaWrap(struct mScriptEngineContextLua* luaContext, struct mScriptValue*);

static void _luaDeref(struct mScriptValue*);

static int _luaThunk(lua_State* lua);
static int _luaGetObject(lua_State* lua);
static int _luaSetObject(lua_State* lua);
static int _luaGcObject(lua_State* lua);
static int _luaGetTable(lua_State* lua);
static int _luaLenTable(lua_State* lua);
static int _luaPairsTable(lua_State* lua);
static int _luaGetList(lua_State* lua);
static int _luaLenList(lua_State* lua);

static int _luaRequireShim(lua_State* lua);

#if LUA_VERSION_NUM < 503
#define lua_pushinteger lua_pushnumber
#endif

const struct mScriptType mSTLuaFunc = {
	.base = mSCRIPT_TYPE_FUNCTION,
	.size = 0,
	.name = "lua-" LUA_VERSION_ONLY "::function",
	.details = {
		.function = {
			.parameters = {
				.variable = true
			},
			.returnType = {
				.variable = true
			}
		}
	},
	.alloc = NULL,
	.free = _luaDeref,
	.hash = NULL,
	.equal = NULL,
	.cast = NULL,
};

struct mScriptEngineContextLua {
	struct mScriptEngineContext d;
	lua_State* lua;
	int func;
	char lastDirectory[PATH_MAX];
	int require;
	char* lastError;
};

struct mScriptEngineContextLuaRef {
	struct mScriptEngineContextLua* context;
	int ref;
};

static struct mScriptEngineLua {
	struct mScriptEngine2 d;
} _engineLua = {
	.d = {
		.name = "lua-" LUA_VERSION_ONLY,
		.init = NULL,
		.deinit = NULL,
		.create = _luaCreate
	}
};

struct mScriptEngine2* const mSCRIPT_ENGINE_LUA = &_engineLua.d;

static const luaL_Reg _mSTStruct[] = {
	{ "__index", _luaGetObject },
	{ "__newindex", _luaSetObject },
	{ "__gc", _luaGcObject },
	{ NULL, NULL }
};

static const luaL_Reg _mSTTable[] = {
	{ "__index", _luaGetTable },
	{ "__len", _luaLenTable },
	{ "__pairs", _luaPairsTable },
	{ "__gc", _luaGcObject },
	{ NULL, NULL }
};

static const luaL_Reg _mSTList[] = {
	{ "__index", _luaGetList },
	{ "__len", _luaLenList },
	{ "__gc", _luaGcObject },
	{ NULL, NULL }
};

struct mScriptEngineContext* _luaCreate(struct mScriptEngine2* engine, struct mScriptContext* context) {
	UNUSED(engine);
	struct mScriptEngineContextLua* luaContext = calloc(1, sizeof(*luaContext));
	luaContext->d = (struct mScriptEngineContext) {
		.context = context,
		.destroy = _luaDestroy,
		.isScript = _luaIsScript,
		.getGlobal = _luaGetGlobal,
		.setGlobal = _luaSetGlobal,
		.load = _luaLoad,
		.run = _luaRun,
		.getError = _luaGetError
	};
	luaContext->lua = luaL_newstate();
	luaContext->func = -1;

	luaL_openlibs(luaContext->lua);

	luaL_newmetatable(luaContext->lua, "mSTStruct");
#if LUA_VERSION_NUM < 502
	luaL_register(luaContext->lua, NULL, _mSTStruct);
#else
	luaL_setfuncs(luaContext->lua, _mSTStruct, 0);
#endif
	lua_pop(luaContext->lua, 1);

	luaL_newmetatable(luaContext->lua, "mSTTable");
#if LUA_VERSION_NUM < 502
	luaL_register(luaContext->lua, NULL, _mSTTable);
#else
	luaL_setfuncs(luaContext->lua, _mSTTable, 0);
#endif
	lua_pop(luaContext->lua, 1);

	luaL_newmetatable(luaContext->lua, "mSTList");
#if LUA_VERSION_NUM < 502
	luaL_register(luaContext->lua, NULL, _mSTList);
#else
	luaL_setfuncs(luaContext->lua, _mSTList, 0);
#endif
	lua_pop(luaContext->lua, 1);

	lua_getglobal(luaContext->lua, "require");
	luaContext->require = luaL_ref(luaContext->lua, LUA_REGISTRYINDEX);

	return &luaContext->d;
}

void _luaDestroy(struct mScriptEngineContext* ctx) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) ctx;
	if (luaContext->lastError) {
		free(luaContext->lastError);
		luaContext->lastError = NULL;
	}
	if (luaContext->func > 0) {
		luaL_unref(luaContext->lua, LUA_REGISTRYINDEX, luaContext->func);
	}
	lua_close(luaContext->lua);
	free(luaContext);
}

bool _luaIsScript(struct mScriptEngineContext* ctx, const char* name, struct VFile* vf) {
	UNUSED(ctx);
	UNUSED(vf);
	if (!name) {
		return false;
	}
	return endswith(name, ".lua");
}

struct mScriptValue* _luaGetGlobal(struct mScriptEngineContext* ctx, const char* name) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) ctx;
	lua_getglobal(luaContext->lua, name);
	return _luaCoerce(luaContext, true);
}

bool _luaSetGlobal(struct mScriptEngineContext* ctx, const char* name, struct mScriptValue* value) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) ctx;
	if (!value) {
		lua_pushnil(luaContext->lua);
	} else if (!_luaWrap(luaContext, value)) {
		return false;
	}
	lua_setglobal(luaContext->lua, name);
	return true;
}

struct mScriptValue* _luaCoerceFunction(struct mScriptEngineContextLua* luaContext) {
	struct mScriptValue* value = mScriptValueAlloc(&mSTLuaFunc);
	struct mScriptFunction* fn = calloc(1, sizeof(*fn));
	struct mScriptEngineContextLuaRef* ref = calloc(1, sizeof(*ref));
	fn->call = _luaCall;
	fn->context = ref;
	ref->context = luaContext;
	ref->ref = luaL_ref(luaContext->lua, LUA_REGISTRYINDEX);
	value->value.opaque = fn;
	return value;
}

struct mScriptValue* _luaCoerceTable(struct mScriptEngineContextLua* luaContext) {
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	bool isList = true;

	lua_pushnil(luaContext->lua);
	while (lua_next(luaContext->lua, -2) != 0) {
		struct mScriptValue* value = NULL;
		int type = lua_type(luaContext->lua, -1);
		switch (type) {
		case LUA_TNUMBER:
		case LUA_TBOOLEAN:
		case LUA_TSTRING:
		case LUA_TFUNCTION:
			value = _luaCoerce(luaContext, true);
			break;
		default:
			// Don't let values be something that could contain themselves
			break;
		}
		if (!value) {
			lua_pop(luaContext->lua, 3);
			mScriptValueDeref(table);
			return false;
		}

		struct mScriptValue* key = NULL;
		type = lua_type(luaContext->lua, -1);
		switch (type) {
		case LUA_TBOOLEAN:
		case LUA_TSTRING:
			isList = false;
			// Fall through
		case LUA_TNUMBER:
			key = _luaCoerce(luaContext, false);
			break;
		default:
			// Limit keys to hashable types
			break;
		}

		if (!key) {
			lua_pop(luaContext->lua, 2);
			mScriptValueDeref(table);
			return false;
		}
		mScriptTableInsert(table, key, value);
		mScriptValueDeref(key);
		mScriptValueDeref(value);
	}
	lua_pop(luaContext->lua, 1);

	size_t len = mScriptTableSize(table);
	if (!isList || !len) {
		return table;
	}

	struct mScriptValue* list = mScriptValueAlloc(mSCRIPT_TYPE_MS_LIST);
	size_t i;
	for (i = 1; i <= len; ++i) {
		struct mScriptValue* value = mScriptTableLookup(table, &mSCRIPT_MAKE_S64(i));
		if (!value) {
			mScriptValueDeref(list);
			return table;
		}
		mScriptValueWrap(value, mScriptListAppend(list->value.list));
	}
	if (i != len + 1) {
		mScriptValueDeref(list);
		mScriptContextFillPool(luaContext->d.context, table);
		return table;
	}
	mScriptValueDeref(table);
	mScriptContextFillPool(luaContext->d.context, list);
	return list;
}

struct mScriptValue* _luaCoerce(struct mScriptEngineContextLua* luaContext, bool pop) {
	if (lua_isnone(luaContext->lua, -1)) {
		lua_pop(luaContext->lua, 1);
		return NULL;
	}

	size_t size;
	const void* buffer;
	struct mScriptValue* value = NULL;
	switch (lua_type(luaContext->lua, -1)) {
	case LUA_TNUMBER:
#if LUA_VERSION_NUM >= 503
		if (lua_isinteger(luaContext->lua, -1)) {
			value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S64);
			value->value.s64 = lua_tointeger(luaContext->lua, -1);
			break;
		}
#endif
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_F64);
		value->value.f64 = lua_tonumber(luaContext->lua, -1);
		break;
	case LUA_TBOOLEAN:
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
		value->value.s32 = lua_toboolean(luaContext->lua, -1);
		break;
	case LUA_TSTRING:
		buffer = lua_tolstring(luaContext->lua, -1, &size);
		value = mScriptStringCreateFromBytes(buffer, size);
		mScriptContextFillPool(luaContext->d.context, value);
		break;
	case LUA_TFUNCTION:
		// This function pops the value internally via luaL_ref
		if (!pop) {
			break;
		}
		return _luaCoerceFunction(luaContext);
	case LUA_TTABLE:
		// This function pops the value internally
		if (!pop) {
			break;
		}
		return _luaCoerceTable(luaContext);
	case LUA_TUSERDATA:
		if (!lua_getmetatable(luaContext->lua, -1)) {
			break;
		}
		luaL_getmetatable(luaContext->lua, "mSTStruct");
		if (!lua_rawequal(luaContext->lua, -1, -2)) {
			lua_pop(luaContext->lua, 2);
			break;
		}
		lua_pop(luaContext->lua, 2);
		value = lua_touserdata(luaContext->lua, -1);
		value = mScriptContextAccessWeakref(luaContext->d.context, value);
		break;
	}
	if (pop) {
		lua_pop(luaContext->lua, 1);
	}
	return value;
}

bool _luaWrap(struct mScriptEngineContextLua* luaContext, struct mScriptValue* value) {
	if (!value) {
		lua_pushnil(luaContext->lua);
		return true;
	}
	uint32_t weakref;
	bool needsWeakref = false;
	if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
		value = mScriptValueUnwrap(value);
		if (!value) {
			lua_pushnil(luaContext->lua);
			return true;
		}
	}
	if (value->type == mSCRIPT_TYPE_MS_WEAKREF) {
		weakref = value->value.u32;
		value = mScriptContextAccessWeakref(luaContext->d.context, value);
		if (!value) {
			lua_pushnil(luaContext->lua);
			return true;
		}
		needsWeakref = true;
	}
	bool ok = true;
	struct mScriptValue* newValue;
	switch (value->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (value->type->size <= 4) {
			lua_pushinteger(luaContext->lua, value->value.s32);
		} else if (value->type->size == 8) {
			lua_pushinteger(luaContext->lua, value->value.s64);
		} else {
			ok = false;
		}
		break;
	case mSCRIPT_TYPE_UINT:
		if (value->type->size <= 4) {
			lua_pushinteger(luaContext->lua, value->value.u32);
		} else if (value->type->size == 8) {
			lua_pushinteger(luaContext->lua, value->value.u64);
		} else {
			ok = false;
		}
		break;
	case mSCRIPT_TYPE_FLOAT:
		if (value->type->size == 4) {
			lua_pushnumber(luaContext->lua, value->value.f32);
		} else if (value->type->size == 8) {
			lua_pushnumber(luaContext->lua, value->value.f64);
		} else {
			ok = false;
		}
		break;
	case mSCRIPT_TYPE_STRING:
		lua_pushlstring(luaContext->lua, value->value.string->buffer, value->value.string->size);
		break;
	case mSCRIPT_TYPE_LIST:
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		if (needsWeakref) {
			*newValue = mSCRIPT_MAKE(WEAKREF, weakref);
		} else {
			mScriptValueWrap(value, newValue);
		}
		luaL_setmetatable(luaContext->lua, "mSTList");
		break;
	case mSCRIPT_TYPE_TABLE:
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		if (needsWeakref) {
			*newValue = mSCRIPT_MAKE(WEAKREF, weakref);
		} else {
			mScriptValueWrap(value, newValue);
		}
		luaL_setmetatable(luaContext->lua, "mSTTable");
		break;
	case mSCRIPT_TYPE_FUNCTION:
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		newValue->type = value->type;
		newValue->refs = mSCRIPT_VALUE_UNREF;
		newValue->type->alloc(newValue);
		lua_pushcclosure(luaContext->lua, _luaThunk, 1);
		mScriptValueDeref(value);
		break;
	case mSCRIPT_TYPE_OBJECT:
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		if (needsWeakref) {
			*newValue = mSCRIPT_MAKE(WEAKREF, weakref);
		} else {
			mScriptValueWrap(value, newValue);
		}
		luaL_setmetatable(luaContext->lua, "mSTStruct");
		break;
	default:
		ok = false;
		break;
	}

	return ok;
}

#define LUA_BLOCKSIZE 0x1000
struct mScriptEngineLuaReader {
	struct VFile* vf;
	char block[LUA_BLOCKSIZE];
};

static const char* _reader(lua_State* lua, void* context, size_t* size) {
	UNUSED(lua);
	struct mScriptEngineLuaReader* reader = context;
	ssize_t s = reader->vf->read(reader->vf, reader->block, sizeof(reader->block));
	if (s < 0) {
		return NULL;
	}
	*size = s;
	return reader->block;
}

bool _luaLoad(struct mScriptEngineContext* ctx, const char* filename, struct VFile* vf) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) ctx;
	struct mScriptEngineLuaReader data = {
		.vf = vf
	};
	if (luaContext->lastError) {
		free(luaContext->lastError);
		luaContext->lastError = NULL;
	}
	char name[PATH_MAX + 1];
	char dirname[PATH_MAX] = {0};
	if (filename) {
		if (*filename == '*') {
			snprintf(name, sizeof(name), "=%s", filename + 1);
		} else {
			const char* lastSlash = strrchr(filename, '/');
			const char* lastBackslash = strrchr(filename, '\\');
			if (lastSlash && lastBackslash) {
				if (lastSlash < lastBackslash) {
					lastSlash = lastBackslash;
				}
			} else if (lastBackslash) {
				lastSlash = lastBackslash;
			}
			if (lastSlash) {
				strncpy(dirname, filename, lastSlash - filename);
			}
			snprintf(name, sizeof(name), "@%s", filename);
		}
		filename = name;
	}
	int ret = lua_load(luaContext->lua, _reader, &data, filename, "t");
	switch (ret) {
	case LUA_OK:
		luaContext->func = luaL_ref(luaContext->lua, LUA_REGISTRYINDEX);
		if (dirname[0]) {
			strncpy(luaContext->lastDirectory, dirname, sizeof(luaContext->lastDirectory));
		} else {
			memset(luaContext->lastDirectory, 0, sizeof(luaContext->lastDirectory));
		}
		return true;
	case LUA_ERRSYNTAX:
		luaContext->lastError = strdup(lua_tostring(luaContext->lua, -1));
		lua_pop(luaContext->lua, 1);
		break;
	default:
		break;
	}
	return false;
}

bool _luaRun(struct mScriptEngineContext* context) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) context;

	if (luaContext->lastDirectory[0]) {
		// Shim require to look in the previous location
		lua_pushstring(luaContext->lua, luaContext->lastDirectory);
		lua_pushcclosure(luaContext->lua, _luaRequireShim, 1);
		lua_setglobal(luaContext->lua, "require");
	}

	lua_rawgeti(luaContext->lua, LUA_REGISTRYINDEX, luaContext->func);
	bool ret = _luaInvoke(luaContext, NULL);

	// Restore previous value of require
	lua_rawgeti(luaContext->lua, LUA_REGISTRYINDEX, luaContext->require);
	lua_setglobal(luaContext->lua, "require");
	return ret;
}

const char* _luaGetError(struct mScriptEngineContext* context) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) context;
	return luaContext->lastError;
}

bool _luaPushFrame(struct mScriptEngineContextLua* luaContext, struct mScriptList* frame, bool internal) {
	bool ok = true;
	if (frame) {
		size_t i;
		for (i = 0; i < mScriptListSize(frame); ++i) {
			struct mScriptValue* value = mScriptListGetPointer(frame, i);
			if (internal && value->type->base == mSCRIPT_TYPE_WRAPPER) {
				value = mScriptValueUnwrap(value);
				mScriptContextFillPool(luaContext->d.context, value);
			}
			if (!_luaWrap(luaContext, value)) {
				ok = false;
				break;
			}
		}
	}
	if (!ok) {
		lua_pop(luaContext->lua, lua_gettop(luaContext->lua));
	}
	return ok;
}

bool _luaPopFrame(struct mScriptEngineContextLua* luaContext, struct mScriptList* frame) {
	int count = lua_gettop(luaContext->lua);
	bool ok = true;
	if (frame) {
		int i;
		for (i = 0; i < count; ++i) {
			struct mScriptValue* value = _luaCoerce(luaContext, true);
			if (!value) {
				ok = false;
				break;
			}
			mScriptValueWrap(value, mScriptListAppend(frame));
			mScriptValueDeref(value);
		}
		if (count > i) {
			lua_pop(luaContext->lua, count - i);
		}

		if (ok) {
			for (i = 0; i < (ssize_t) (mScriptListSize(frame) / 2); ++i) {
				struct mScriptValue buffer;
				memcpy(&buffer, mScriptListGetPointer(frame, i), sizeof(buffer));
				memcpy(mScriptListGetPointer(frame, i), mScriptListGetPointer(frame, mScriptListSize(frame) - i - 1), sizeof(buffer));
				memcpy(mScriptListGetPointer(frame, mScriptListSize(frame) - i - 1), &buffer, sizeof(buffer));
			}
		}
	}
	return ok;
}

bool _luaCall(struct mScriptFrame* frame, void* context) {
	struct mScriptEngineContextLuaRef* ref = context;
	lua_rawgeti(ref->context->lua, LUA_REGISTRYINDEX, ref->ref);
	if (!_luaInvoke(ref->context, frame)) {
		return false;
	}
	return true;
}

bool _luaInvoke(struct mScriptEngineContextLua* luaContext, struct mScriptFrame* frame) {
	int nargs = 0;
	if (frame) {
		nargs = mScriptListSize(&frame->arguments);
	}

	if (luaContext->lastError) {
		free(luaContext->lastError);
		luaContext->lastError = NULL;
	}

	if (frame && !_luaPushFrame(luaContext, &frame->arguments, false)) {
		return false;
	}

	lua_pushliteral(luaContext->lua, "mCtx");
	lua_pushlightuserdata(luaContext->lua, luaContext);
	lua_rawset(luaContext->lua, LUA_REGISTRYINDEX);
	int ret = lua_pcall(luaContext->lua, nargs, LUA_MULTRET, 0);
	lua_pushliteral(luaContext->lua, "mCtx");
	lua_pushnil(luaContext->lua);
	lua_rawset(luaContext->lua, LUA_REGISTRYINDEX);

	if (ret == LUA_ERRRUN) {
		luaContext->lastError = strdup(lua_tostring(luaContext->lua, -1));
		lua_pop(luaContext->lua, 1);
	}
	if (ret) {
		return false;
	}

	if (frame && !_luaPopFrame(luaContext, &frame->returnValues)) {
		mScriptContextDrainPool(luaContext->d.context);
		return false;
	}
	mScriptContextDrainPool(luaContext->d.context);

	return true;
}

void _luaDeref(struct mScriptValue* value) {
	struct mScriptEngineContextLuaRef* ref;
	if (value->type->base == mSCRIPT_TYPE_FUNCTION) {
		struct mScriptFunction* function = value->value.opaque;
		ref = function->context;
		free(function);
	} else {
		return;
	}
	luaL_unref(ref->context->lua, LUA_REGISTRYINDEX, ref->ref);
	free(ref);
}

static struct mScriptEngineContextLua* _luaGetContext(lua_State* lua) {
	lua_pushliteral(lua, "mCtx");
	int type = lua_rawget(lua, LUA_REGISTRYINDEX);
	if (type != LUA_TLIGHTUSERDATA) {
		lua_pop(lua, 1);
		lua_pushliteral(lua, "Function called from invalid context");
		lua_error(lua);
	}

	struct mScriptEngineContextLua* luaContext = lua_touserdata(lua, -1);
	lua_pop(lua, 1);
	if (luaContext->lua != lua) {
		lua_pushliteral(lua, "Function called from invalid context");
		lua_error(lua);
	}
	return luaContext;
}

int _luaThunk(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	if (!_luaPopFrame(luaContext, &frame.arguments)) {
		mScriptContextDrainPool(luaContext->d.context);
		mScriptFrameDeinit(&frame);
		luaL_traceback(lua, lua, "Error calling function (translating arguments into runtime)", 1);
		lua_error(lua);
	}

	struct mScriptValue* fn = lua_touserdata(lua, lua_upvalueindex(1));
	if (!fn || !mScriptInvoke(fn, &frame)) {
		mScriptFrameDeinit(&frame);
		luaL_traceback(lua, lua, "Error calling function (invoking failed)", 1);
		lua_error(lua);
	}

	if (!_luaPushFrame(luaContext, &frame.returnValues, true)) {
		mScriptFrameDeinit(&frame);
		luaL_traceback(lua, lua, "Error calling function (translating return values from runtime)", 1);
		lua_error(lua);
	}
	mScriptContextDrainPool(luaContext->d.context);
	mScriptFrameDeinit(&frame);

	return lua_gettop(luaContext->lua);
}

int _luaGetObject(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	char key[MAX_KEY_SIZE];
	const char* keyPtr = lua_tostring(lua, -1);
	struct mScriptValue* obj = lua_touserdata(lua, -2);
	struct mScriptValue val;

	if (!keyPtr) {
		lua_pop(lua, 2);
		luaL_traceback(lua, lua, "Invalid key", 1);
		lua_error(lua);
	}
	strlcpy(key, keyPtr, sizeof(key));
	lua_pop(lua, 2);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (!obj) {
		luaL_traceback(lua, lua, "Invalid object", 1);
		lua_error(lua);
	}

	if (!mScriptObjectGet(obj, key, &val)) {
		char error[MAX_KEY_SIZE + 16];
		snprintf(error, sizeof(error), "Invalid key '%s'", key);
		luaL_traceback(lua, lua, "Invalid key", 1);
		lua_error(lua);
	}

	if (!_luaWrap(luaContext, &val)) {
		luaL_traceback(lua, lua, "Error translating value from runtime", 1);
		lua_error(lua);
	}
	return 1;
}

int _luaSetObject(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	char key[MAX_KEY_SIZE];
	const char* keyPtr = lua_tostring(lua, -2);
	struct mScriptValue* obj = lua_touserdata(lua, -3);
	struct mScriptValue* val = _luaCoerce(luaContext, true);

	if (!keyPtr) {
		lua_pop(lua, 2);
		luaL_traceback(lua, lua, "Invalid key", 1);
		lua_error(lua);
	}
	strlcpy(key, keyPtr, sizeof(key));
	lua_pop(lua, 2);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (!obj) {
		luaL_traceback(lua, lua, "Invalid object", 1);
		lua_error(lua);
	}

	if (!val) {
		luaL_traceback(lua, lua, "Error translating value to runtime", 1);
		lua_error(lua);
	}

	if (!mScriptObjectSet(obj, key, val)) {
		mScriptValueDeref(val);
		char error[MAX_KEY_SIZE + 16];
		snprintf(error, sizeof(error), "Invalid key '%s'", key);
		luaL_traceback(lua, lua, "Invalid key", 1);
		lua_error(lua);
	}
	mScriptValueDeref(val);
	mScriptContextDrainPool(luaContext->d.context);
	return 0;
}

static int _luaGcObject(lua_State* lua) {
	struct mScriptValue* val = lua_touserdata(lua, -1);
	val = mScriptValueUnwrap(val);
	if (!val) {
		return 0;
	}
	mScriptValueDeref(val);
	return 0;
}

int _luaGetTable(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	char key[MAX_KEY_SIZE];
	int type = lua_type(luaContext->lua, -1);
	const char* keyPtr = NULL;
	int64_t intKey;
	switch (type) {
	case LUA_TNUMBER:
		intKey = lua_tointeger(luaContext->lua, -1);
		break;
	case LUA_TSTRING:
		keyPtr = lua_tostring(lua, -1);
		break;
	default:
		lua_pop(lua, 2);
		return 0;
	}
	struct mScriptValue* obj = lua_touserdata(lua, -2);
	if (keyPtr) {
		strlcpy(key, keyPtr, sizeof(key));
	}
	lua_pop(lua, 2);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (!obj) {
		luaL_traceback(lua, lua, "Invalid table", 1);
		lua_error(lua);
	}

	struct mScriptValue keyVal;
	switch (type) {
	case LUA_TNUMBER:
		keyVal = mSCRIPT_MAKE_S64(intKey);
		break;
	case LUA_TSTRING:
		keyVal = mSCRIPT_MAKE_CHARP(key);
		break;
	}
	struct mScriptValue* val = mScriptTableLookup(obj, &keyVal);
	if (!val) {
		return 0;
	}

	if (!_luaWrap(luaContext, val)) {
		luaL_traceback(lua, lua, "Error translating value from runtime", 1);
		lua_error(lua);
	}
	return 1;
}

int _luaLenTable(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	struct mScriptValue* obj = lua_touserdata(lua, -1);
	lua_pop(lua, 1);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (!obj) {
		luaL_traceback(lua, lua, "Invalid table", 1);
		lua_error(lua);
	}

	struct mScriptValue val = mSCRIPT_MAKE_U64(mScriptTableSize(obj));

	if (!_luaWrap(luaContext, &val)) {
		luaL_traceback(lua, lua, "Error translating value from runtime", 1);
		lua_error(lua);
	}
	return 1;
}

static int _luaNextTable(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	char key[MAX_KEY_SIZE];
	int type = lua_type(luaContext->lua, -1);
	const char* keyPtr = NULL;
	struct mScriptValue keyVal = {0};
	switch (type) {
	case LUA_TNUMBER:
		keyVal = mSCRIPT_MAKE_S64(lua_tointeger(luaContext->lua, -1));
		break;
	case LUA_TSTRING:
		keyPtr = lua_tostring(lua, -1);
		break;
	}
	struct mScriptValue* table = lua_touserdata(lua, -2);
	if (keyPtr) {
		strlcpy(key, keyPtr, sizeof(key));
		keyVal = mSCRIPT_MAKE_CHARP(key);
	}
	lua_pop(lua, 2);

	table = mScriptContextAccessWeakref(luaContext->d.context, table);
	if (!table) {
		luaL_traceback(lua, lua, "Invalid table", 1);
		lua_error(lua);
	}

	struct TableIterator iter;
	if (keyVal.type) {
		if (!mScriptTableIteratorLookup(table, &iter, &keyVal)) {
			return 0;
		}
		if (!mScriptTableIteratorNext(table, &iter)) {
			return 0;
		}
	} else {
		if (!mScriptTableIteratorStart(table, &iter)) {
			return 0;
		}
	}

	if (!_luaWrap(luaContext, mScriptTableIteratorGetKey(table, &iter))) {
		luaL_traceback(lua, lua, "Iteration error", 1);
		lua_error(lua);
	}

	if (!_luaWrap(luaContext, mScriptTableIteratorGetValue(table, &iter))) {
		luaL_traceback(lua, lua, "Iteration error", 1);
		lua_error(lua);
	}

	return 2;
}

int _luaPairsTable(lua_State* lua) {
	lua_pushcfunction(lua, _luaNextTable);
	lua_insert(lua, -2);
	lua_pushnil(lua);
	return 3;
}

int _luaGetList(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	ssize_t index;
#if LUA_VERSION_NUM >= 503
	index = lua_tointeger(luaContext->lua, -1);
#else
	index = lua_tonumber(luaContext->lua, -1);
#endif
	struct mScriptValue* obj = lua_touserdata(lua, -2);
	lua_pop(lua, 2);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (obj->type->base == mSCRIPT_TYPE_WRAPPER) {
		obj = mScriptValueUnwrap(obj);
	}
	if (!obj || obj->type != mSCRIPT_TYPE_MS_LIST) {
		luaL_traceback(lua, lua, "Invalid list", 1);
		lua_error(lua);
	}
	struct mScriptList* list = obj->value.list;

	// Lua indexes from 1
	if (index < 1) {
		luaL_traceback(lua, lua, "Invalid index", 1);
		lua_error(lua);
	}
	if ((size_t) index > mScriptListSize(list)) {
		return 0;
	}
	--index;

	struct mScriptValue* val = mScriptListGetPointer(list, index);
	if (!_luaWrap(luaContext, val)) {
		luaL_traceback(lua, lua, "Error translating value from runtime", 1);
		lua_error(lua);
	}
	return 1;
}

static int _luaLenList(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	struct mScriptValue* obj = lua_touserdata(lua, -1);
	lua_pop(lua, 1);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (obj->type->base == mSCRIPT_TYPE_WRAPPER) {
		obj = mScriptValueUnwrap(obj);
	}
	if (!obj || obj->type != mSCRIPT_TYPE_MS_LIST) {
		luaL_traceback(lua, lua, "Invalid list", 1);
		lua_error(lua);
	}
	struct mScriptList* list = obj->value.list;
	lua_pushinteger(lua, mScriptListSize(list));
	return 1;
}

static int _luaRequireShim(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);

	const char* path = lua_tostring(lua, lua_upvalueindex(1));

	lua_getglobal(luaContext->lua, "package");

	lua_pushliteral(luaContext->lua, "path");
	lua_pushliteral(luaContext->lua, "path");
	lua_gettable(luaContext->lua, -3);
	char* oldpath = strdup(lua_tostring(luaContext->lua, -1));
	lua_pushliteral(luaContext->lua, ";");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?.lua;");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?/init.lua");
	lua_concat(luaContext->lua, 6);
	lua_settable(luaContext->lua, -3);

#ifdef _WIN32
#define DLL "dll"
#elif defined(__APPLE__)
#define DLL "dylib"
#else
#define DLL "so"
#endif
	lua_pushliteral(luaContext->lua, "cpath");
	lua_pushliteral(luaContext->lua, "cpath");
	lua_gettable(luaContext->lua, -3);
	char* oldcpath = strdup(lua_tostring(luaContext->lua, -1));
	lua_pushliteral(luaContext->lua, ";");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?." DLL ";");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?/init." DLL);
	lua_concat(luaContext->lua, 6);
	lua_settable(luaContext->lua, -3);

	lua_pop(luaContext->lua, 1);

	lua_rawgeti(luaContext->lua, LUA_REGISTRYINDEX, luaContext->require);
	lua_rotate(luaContext->lua, -2, 1);
	int ret = lua_pcall(luaContext->lua, 1, 0, 0);

	lua_getglobal(luaContext->lua, "package");

	lua_pushliteral(luaContext->lua, "path");
	lua_pushstring(luaContext->lua, oldpath);
	lua_settable(luaContext->lua, -3);

	lua_pushliteral(luaContext->lua, "cpath");
	lua_pushstring(luaContext->lua, oldcpath);
	lua_settable(luaContext->lua, -3);

	lua_pop(luaContext->lua, 1);

	free(oldpath);
	free(oldcpath);
	if (ret) {
		lua_error(luaContext->lua);
	}

	return 0;
}
