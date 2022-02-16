/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/script/lua.h>
#include <lauxlib.h>

static struct mScriptEngineContext* _luaCreate(struct mScriptEngine2*, struct mScriptContext*);

static void _luaDestroy(struct mScriptEngineContext*);
static bool _luaLoad(struct mScriptEngineContext*, struct VFile*, const char** error);

struct mScriptEngineContextLua {
	struct mScriptEngineContext d;
	lua_State* lua;
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

struct mScriptEngineContext* _luaCreate(struct mScriptEngine2* engine, struct mScriptContext* context) {
	UNUSED(engine);
	struct mScriptEngineContextLua* luaContext = calloc(1, sizeof(*luaContext));
	luaContext->d = (struct mScriptEngineContext) {
		.context = context,
		.destroy = _luaDestroy,
		.load = _luaLoad,
	};
	luaContext->lua = luaL_newstate();
	return &luaContext->d;
}

void _luaDestroy(struct mScriptEngineContext* ctx) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) ctx;
	lua_close(luaContext->lua);
	free(luaContext);
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

bool _luaLoad(struct mScriptEngineContext* ctx, struct VFile* vf, const char** error) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) ctx;
	struct mScriptEngineLuaReader data = {
		.vf = vf
	};
	int ret = lua_load(luaContext->lua, _reader, &data, NULL, "t");
	switch (ret) {
	case LUA_OK:
		if (error) {
			*error = NULL;
		}
		return true;
	case LUA_ERRSYNTAX:
		if (error) {
			*error = "Syntax error";
		}
		break;
	default:
		if (error) {
			*error = "Unknown error";
		}
		break;
	}
	return false;
}
