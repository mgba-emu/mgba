/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/script/lua.h>

#include <mgba/internal/script/socket.h>
#include <mgba/script/context.h>
#include <mgba/script/macros.h>
#include <mgba/script/types.h>
#include <mgba-util/string.h>

#include <lualib.h>
#include <lauxlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_KEY_SIZE 128
#define LUA_NAME "lua"

#define mSCRIPT_TYPE_MS_LUA_FUNC (&mSTLuaFunc)

static struct mScriptEngineContext* _luaCreate(struct mScriptEngine2*, struct mScriptContext*);

static void _luaDestroy(struct mScriptEngineContext*);
static bool _luaIsScript(struct mScriptEngineContext*, const char*, struct VFile*);
static struct mScriptValue* _luaGetGlobal(struct mScriptEngineContext*, const char* name);
static bool _luaSetGlobal(struct mScriptEngineContext*, const char* name, struct mScriptValue*);
static struct mScriptValue* _luaRootScope(struct mScriptEngineContext*);
static bool _luaLoad(struct mScriptEngineContext*, const char*, struct VFile*);
static bool _luaRun(struct mScriptEngineContext*);
static const char* _luaGetError(struct mScriptEngineContext*);

static bool _luaCall(struct mScriptFrame*, void* context);

static void _freeFrame(struct mScriptList* frame);
static void _autofreeFrame(struct mScriptContext* context, struct mScriptList* frame);

struct mScriptEngineContextLua;
static bool _luaPushFrame(struct mScriptEngineContextLua*, struct mScriptList*);
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

static const char* _socketLuaSource =
	"socket = {\n"
	"  ERRORS = {},\n"
	"  tcp = function() return socket._create(_socket.create(), socket._tcpMT) end,\n"
	"  bind = function(address, port)\n"
	"    local s = socket.tcp()\n"
	"    local ok, err = s:bind(address, port)\n"
	"    if ok then return s end\n"
	"    return ok, err\n"
	"  end,\n"
	"  connect = function(address, port)\n"
	"    local s = socket.tcp()\n"
	"    local ok, err = s:connect(address, port)\n"
	"    if ok then return s end\n"
	"    return ok, err\n"
	"  end,\n"
	"  _create = function(sock, mt) return setmetatable({\n"
	"    _s = sock,\n"
	"    _callbacks = {},\n"
	"    _nextCallback = 1,\n"
	"  }, mt) end,\n"
	"  _wrap = function(status)\n"
	"    if status == 0 then return 1 end\n"
	"    return nil, socket.ERRORS[status] or ('error#' .. status)\n"
	"  end,\n"
	"  _mt = {\n"
	"    __index = {\n"
	"      close = function(self)\n"
	"        if self._onframecb then\n"
	"          callbacks:remove(self._onframecb)\n"
	"          self._onframecb = nil\n"
	"        end\n"
	"        self._callbacks = {}\n"
	"        return self._s:close()\n"
	"      end,\n"
	"      add = function(self, event, callback)\n"
	"        if not self._callbacks[event] then self._callbacks[event] = {} end\n"
	"        local cbid = self._nextCallback\n"
	"        self._nextCallback = cbid + 1\n"
	"        self._callbacks[event][cbid] = callback\n"
	"        return cbid\n"
	"      end,\n"
	"      remove = function(self, cbid)\n"
	"        for _, group in pairs(self._callbacks) do\n"
	"          if group[cbid] then\n"
	"            group[cbid] = nil\n"
	"          end\n"
	"        end\n"
	"      end,\n"
	"      _dispatch = function(self, event, ...)\n"
	"        if not self._callbacks[event] then return end\n"
	"        for k, cb in pairs(self._callbacks[event]) do\n"
	"          if cb then\n"
	"            local ok, ret = pcall(cb, self, ...)\n"
	"            if not ok then console:error(ret) end\n"
	"          end\n"
	"        end\n"
	"      end,\n"
	"    },\n"
	"  },\n"
	"  _tcpMT = {\n"
	"    __index = {\n"
	"      _hook = function(self, status)\n"
	"        if status == 0 then\n"
	"          self._onframecb = callbacks:add('frame', function() self:poll() end)\n"
	"        end\n"
	"        return socket._wrap(status)\n"
	"      end,\n"
	"      bind = function(self, address, port)\n"
	"        return socket._wrap(self._s:open(address or '', port))\n"
	"      end,\n"
	"      connect = function(self, address, port)\n"
	"        local status = self._s:connect(address, port)\n"
	"        return self:_hook(status)\n"
	"      end,\n"
	"      listen = function(self, backlog)\n"
	"        local status = self._s:listen(backlog or 1)\n"
	"        return self:_hook(status)\n"
	"      end,\n"
	"      accept = function(self)\n"
	"        local client = self._s:accept()\n"
	"        if client.error ~= 0 then\n"
	"          client:close()\n"
	"          return socket._wrap(client.error)\n"
	"        end\n"
	"        local sock = socket._create(client, socket._tcpMT)\n"
	"        sock:_hook(0)\n"
	"        return sock\n"
	"      end,\n"
	"      send = function(self, data, i, j)\n"
	"        local result = self._s:send(string.sub(data, i or 1, j))\n"
	"        if result < 0 then return socket._wrap(self._s.error) end\n"
	"        if i then return result + i - 1 end\n"
	"        return result\n"
	"      end,\n"
	// TODO: This does not match the API for LuaSocket's receive() implementation
	"      receive = function(self, maxBytes)\n"
	"        local result = self._s:recv(maxBytes)\n"
	"        if (not result or #result == 0) and self._s.error ~= 0 then\n"
	"          return socket._wrap(self._s.error)\n"
	"        elseif not result or #result == 0 then\n"
	"          return nil, 'disconnected'\n"
	"        end\n"
	"        return result or ''\n"
	"      end,\n"
	"      hasdata = function(self)\n"
	"        local status = self._s:select(0)\n"
	"        if status < 0 then\n"
	"          return socket._wrap(self._s.error)\n"
	"        end\n"
	"        return status > 0\n"
	"      end,\n"
	"      poll = function(self)\n"
	"        local status, err = self:hasdata()\n"
	"        if err then\n"
	"          self:_dispatch('error', err)\n"
	"        elseif status then\n"
	"          self:_dispatch('received')\n"
	"        end\n"
	"      end,\n"
	"    },\n"
	"  },\n"
	"  _errMT = {\n"
	"    __index = function (tbl, key)\n"
	"      return rawget(tbl, C.SOCKERR[key])\n"
	"    end,\n"
	"  },\n"
	"}\n"
	"setmetatable(socket._tcpMT.__index, socket._mt)\n"
	"setmetatable(socket.ERRORS, socket._errMT)\n";

static const struct _mScriptSocketError {
	enum mSocketErrorCode err;
	const char* message;
} _mScriptSocketErrors[] = {
	{ mSCRIPT_SOCKERR_UNKNOWN_ERROR, "unknown error" },
	{ mSCRIPT_SOCKERR_OK, NULL },
	{ mSCRIPT_SOCKERR_AGAIN, "temporary failure" },
	{ mSCRIPT_SOCKERR_ADDRESS_IN_USE, "address in use" },
	{ mSCRIPT_SOCKERR_DENIED, "access denied" },
	{ mSCRIPT_SOCKERR_UNSUPPORTED, "unsupported" },
	{ mSCRIPT_SOCKERR_CONNECTION_REFUSED, "connection refused" },
	{ mSCRIPT_SOCKERR_NETWORK_UNREACHABLE, "network unreachable" },
	{ mSCRIPT_SOCKERR_TIMEOUT, "timeout" },
	{ mSCRIPT_SOCKERR_FAILED, "failed" },
	{ mSCRIPT_SOCKERR_NOT_FOUND, "not found" },
	{ mSCRIPT_SOCKERR_NO_DATA, "no data" },
	{ mSCRIPT_SOCKERR_OUT_OF_MEMORY, "out of memory" },
};
static const int _mScriptSocketNumErrors = sizeof(_mScriptSocketErrors) / sizeof(struct _mScriptSocketError);

#if LUA_VERSION_NUM < 503
#define lua_pushinteger lua_pushnumber
#endif

#ifndef LUA_OK
#define LUA_OK 0
#endif

#if LUA_VERSION_NUM < 502
#define luaL_traceback(L, M, S, level) lua_pushstring(L, S)
#define lua_pushglobaltable(L) lua_pushvalue(L, LUA_GLOBALSINDEX)
#endif

const struct mScriptType mSTLuaFunc;

mSCRIPT_DECLARE_DOC_STRUCT(LUA_NAME, socket);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD(LUA_NAME, socket, S64, add, 2, STR, event, LUA_FUNC, callback);
mSCRIPT_DECLARE_DOC_STRUCT_VOID_METHOD(LUA_NAME, socket, remove, 1, S64, cbid);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD(LUA_NAME, socket, S32, bind, 2, STR, address, U16, port);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD(LUA_NAME, socket, S32, connect, 2, STR, address, U16, port);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD_WITH_DEFAULTS(LUA_NAME, socket, S32, listen, 1, S32, backlog);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD(LUA_NAME, socket, DS(socket), accept, 0);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD_WITH_DEFAULTS(LUA_NAME, socket, S32, send, 3, STR, data, S64, i, S64, j);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD(LUA_NAME, socket, STR, receive, 1, S64, maxBytes);
mSCRIPT_DECLARE_DOC_STRUCT_METHOD(LUA_NAME, socket, BOOL, hasdata, 0);
mSCRIPT_DECLARE_DOC_STRUCT_VOID_METHOD(LUA_NAME, socket, poll, 0);

mSCRIPT_DEFINE_DOC_STRUCT(LUA_NAME, socket)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"An instance of a TCP socket. Most of these functions will return two values if an error occurs; "
		"the first value is `nil` and the second value is an error string from socket.ERRORS"
	)
	mSCRIPT_DEFINE_DOCSTRING(
		"Add a callback for a named event. The returned id can be used to remove it later. "
		"Events get checked once per frame but can be checked manually using " LUA_NAME "::struct::socket.poll. "
		"The following callbacks are defined:\n\n"
		"- **received**: New data has been received and can be read\n"
		"- **error**: An error has occurred on the socket\n"
	)
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, add)
	mSCRIPT_DEFINE_DOCSTRING("Remove a callback with the previously returned id")
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, remove)
	mSCRIPT_DEFINE_DOCSTRING("Creates a new socket for an incoming connection from a listening server socket")
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, accept)
	mSCRIPT_DEFINE_DOCSTRING("Bind the socket to a specific interface and port. Use `nil` for `address` to bind to all interfaces")
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, bind)
	mSCRIPT_DEFINE_DOCSTRING(
		"Opens a TCP connection to the specified address and port.\n\n"
		"**Caution:** This is a blocking call. The emulator will not respond until "
		"the connection either succeeds or fails"
	)
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, connect)
	mSCRIPT_DEFINE_DOCSTRING(
		"Begins listening for incoming connections. The socket must have first been "
		"bound with the " LUA_NAME "::struct::socket.bind function"
	)
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, listen)
	mSCRIPT_DEFINE_DOCSTRING(
		"Writes a string to the socket. If `i` and `j` are provided, they have the same semantics "
		"as the parameters to `string.sub` to write a substring. Returns the last index written"
	)
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, send)
	mSCRIPT_DEFINE_DOCSTRING(
		"Read up to `maxBytes` bytes from the socket and return them. "
		"If the socket has been disconnected or an error occurs, it will return `nil, error` instead"
	)
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, receive)
	mSCRIPT_DEFINE_DOCSTRING("Check if a socket has data ready to receive, and return true if so")
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, hasdata)
	mSCRIPT_DEFINE_DOCSTRING("Manually check for events on this socket and dispatch associated callbacks")
	mSCRIPT_DEFINE_DOC_STRUCT_METHOD(LUA_NAME, socket, poll)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_DOC_STRUCT_BINDING_DEFAULTS(LUA_NAME, socket, listen)
	mSCRIPT_S32(1),
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_DOC_STRUCT_BINDING_DEFAULTS(LUA_NAME, socket, send)
	mSCRIPT_NO_DEFAULT,
	mSCRIPT_S64(0),
	mSCRIPT_S64(0),
mSCRIPT_DEFINE_DEFAULTS_END;

mSCRIPT_DEFINE_DOC_FUNCTION(LUA_NAME, socket_tcp, DS(socket), 0);
mSCRIPT_DEFINE_DOC_FUNCTION(LUA_NAME, socket_bind, DS(socket), 2, STR, address, U16, port);
mSCRIPT_DEFINE_DOC_FUNCTION(LUA_NAME, socket_connect, DS(socket), 2, STR, address, U16, port);

const struct mScriptType mSTLuaFunc = {
	.base = mSCRIPT_TYPE_FUNCTION,
	.size = 0,
	.name = LUA_NAME "::function",
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
		.name = LUA_NAME,
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
	struct mScriptEngineContextLua* luaContext = calloc(1, sizeof(*luaContext));
	luaContext->d = (struct mScriptEngineContext) {
		.context = context,
		.engine = engine,
		.destroy = _luaDestroy,
		.isScript = _luaIsScript,
		.getGlobal = _luaGetGlobal,
		.setGlobal = _luaSetGlobal,
		.rootScope = _luaRootScope,
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

	HashTableInit(&luaContext->d.docroot, 0, (void (*)(void*)) mScriptValueDeref);

	int status = luaL_dostring(luaContext->lua, _socketLuaSource);
	if (status) {
		mLOG(SCRIPT, ERROR, "Error in dostring while initializing sockets: %s\n", lua_tostring(luaContext->lua, -1));
		lua_pop(luaContext->lua, 1);
	} else {
		struct mScriptValue* errors = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
		int i;
		lua_getglobal(luaContext->lua, "socket");
		lua_getfield(luaContext->lua, -1, "ERRORS");
		for (i = 0; i < _mScriptSocketNumErrors; i++) {
			const struct _mScriptSocketError* err = &_mScriptSocketErrors[i];
			if (err->message) {
				lua_pushstring(luaContext->lua, err->message);
				struct mScriptValue* key = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
				key->value.s32 = err->err;
				struct mScriptValue* message = mScriptStringCreateFromASCII(err->message);
				mScriptTableInsert(errors, key, message);
				mScriptValueDeref(key);
				mScriptValueDeref(message);
			} else {
				lua_pushnil(luaContext->lua);
			}
			lua_seti(luaContext->lua, -2, err->err);
		}
		lua_pop(luaContext->lua, 2);

		mScriptEngineExportDocNamespace(&luaContext->d, "socket", (struct mScriptKVPair[]) {
			mSCRIPT_KV_PAIR(ERRORS, errors),
			mSCRIPT_KV_PAIR(tcp, mSCRIPT_VALUE_DOC_FUNCTION(socket_tcp)),
			mSCRIPT_KV_PAIR(bind, mSCRIPT_VALUE_DOC_FUNCTION(socket_bind)),
			mSCRIPT_KV_PAIR(connect, mSCRIPT_VALUE_DOC_FUNCTION(socket_connect)),
			mSCRIPT_KV_SENTINEL
		});
		mScriptEngineSetDocstring(&luaContext->d, "socket", "A basic TCP socket library");
		mScriptEngineSetDocstring(&luaContext->d, "socket.ERRORS",
			"Error strings corresponding to the C.SOCKERR error codes, indexed both by name and by value");
		mScriptEngineSetDocstring(&luaContext->d, "socket.tcp",
			"Create a new TCP socket, for use with either " LUA_NAME "::struct::socket.bind or " LUA_NAME "::struct::socket.connect later");
		mScriptEngineSetDocstring(&luaContext->d, "socket.bind",
			"Create and bind a new socket to a specific interface and port. "
			"Use `nil` for `address` to bind to all interfaces");
		mScriptEngineSetDocstring(&luaContext->d, "socket.connect",
			"Create and return a new TCP socket with a connection to the specified address and port.\n\n"
			"**Caution:** This is a blocking call. The emulator will not respond until "
			"the connection either succeeds or fails");
	}

	mScriptEngineExportDocNamespace(&luaContext->d, "script", (struct mScriptKVPair[]) {
		mSCRIPT_KV_PAIR(dir, mScriptStringCreateFromASCII("/")),
		mSCRIPT_KV_PAIR(path, mScriptStringCreateFromASCII("/lua")),
		mSCRIPT_KV_SENTINEL
	});

	mScriptEngineSetDocstring(&luaContext->d, "script", "Information about the currently loaded script");
	mScriptEngineSetDocstring(&luaContext->d, "script.dir", "The path to the directory containing the script");
	mScriptEngineSetDocstring(&luaContext->d, "script.path", "The path of the current script file");

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
	if (luaContext->require > 0) {
		luaL_unref(luaContext->lua, LUA_REGISTRYINDEX, luaContext->require);
	}
	lua_close(luaContext->lua);

	HashTableDeinit(&luaContext->d.docroot);
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

struct mScriptValue* _luaRootScope(struct mScriptEngineContext* ctx) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) ctx;

	struct mScriptValue* list = mScriptValueAlloc(mSCRIPT_TYPE_MS_LIST);
	lua_pushglobaltable(luaContext->lua);
	lua_pushnil(luaContext->lua);
	while (lua_next(luaContext->lua, -2) != 0) {
		struct mScriptValue* key;

		lua_pop(luaContext->lua, 1);
		key = _luaCoerce(luaContext, false);
		mScriptValueWrap(key, mScriptListAppend(list->value.list));
		mScriptValueRef(key);
		mScriptContextFillPool(luaContext->d.context, key);
	}
	lua_pop(luaContext->lua, 1);

	return list;
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

struct mScriptValue* _luaCoerceTable(struct mScriptEngineContextLua* luaContext, struct Table* markedObjects) {
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	bool isList = true;

	lua_pushnil(luaContext->lua);

	const void* tablePointer;
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
		case LUA_TTABLE:
			tablePointer = lua_topointer(luaContext->lua, -1);
			// Ensure this table doesn't contain any cycles
			if (!HashTableLookupBinary(markedObjects, &tablePointer, sizeof(tablePointer))) {
				HashTableInsertBinary(markedObjects, &tablePointer, sizeof(tablePointer), (void*) tablePointer);
				value = _luaCoerceTable(luaContext, markedObjects);
			}
		default:
			break;
		}
		if (!value) {
			lua_pop(luaContext->lua, type == LUA_TTABLE ? 2 : 3);
			mScriptValueDeref(table);
			return NULL;
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
		return table;
	}
	for (i = 0; i < mScriptListSize(list->value.list); ++i) {
		struct mScriptValue* value = mScriptListGetPointer(list->value.list, i);
		if (value->type->base != mSCRIPT_TYPE_WRAPPER) {
			continue;
		}
		value = mScriptValueUnwrap(value);
		mScriptValueRef(value);
	}
	mScriptValueDeref(table);
	return list;
}

struct mScriptValue* _luaCoerce(struct mScriptEngineContextLua* luaContext, bool pop) {
	if (lua_isnone(luaContext->lua, -1)) {
		lua_pop(luaContext->lua, 1);
		return NULL;
	}

	size_t size;
	const void* buffer;
	struct Table markedObjects;
	struct mScriptValue* value = NULL;
	switch (lua_type(luaContext->lua, -1)) {
	case LUA_TNIL:
		value = &mScriptValueNull;
		break;
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
		value = mScriptValueAlloc(mSCRIPT_TYPE_MS_BOOL);
		value->value.u32 = lua_toboolean(luaContext->lua, -1);
		break;
	case LUA_TSTRING:
		buffer = lua_tolstring(luaContext->lua, -1, &size);
		value = mScriptStringCreateFromBytes(buffer, size);
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
		HashTableInit(&markedObjects, 0, NULL);
		value = _luaCoerceTable(luaContext, &markedObjects);
		HashTableDeinit(&markedObjects);
		return value;
	case LUA_TUSERDATA:
		if (!lua_getmetatable(luaContext->lua, -1)) {
			break;
		}
		luaL_getmetatable(luaContext->lua, "mSTStruct");
		if (!lua_rawequal(luaContext->lua, -1, -2)) {
			lua_pop(luaContext->lua, 1);
			luaL_getmetatable(luaContext->lua, "mSTList");
			if (!lua_rawequal(luaContext->lua, -1, -2)) {
				lua_pop(luaContext->lua, 1);
				luaL_getmetatable(luaContext->lua, "mSTTable");
				if (!lua_rawequal(luaContext->lua, -1, -2)) {
					lua_pop(luaContext->lua, 2);
					break;
				}
			}
		}
		lua_pop(luaContext->lua, 2);
		value = lua_touserdata(luaContext->lua, -1);
		value = mScriptContextAccessWeakref(luaContext->d.context, value);
		if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
			value = mScriptValueUnwrap(value);
		}
		if (value) {
			mScriptValueRef(value);
		}
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
		mScriptContextFillPool(luaContext->d.context, value);
	}
	struct mScriptValue derefPtr;
	if (value->type->base == mSCRIPT_TYPE_OPAQUE) {
		if (!value->type->details.type) {
			return false;
		}
		mScriptValueFollowPointer(value, &derefPtr);
		switch (derefPtr.type->base) {
		case mSCRIPT_TYPE_VOID:
		case mSCRIPT_TYPE_SINT:
		case mSCRIPT_TYPE_UINT:
		case mSCRIPT_TYPE_FLOAT:
			value = &derefPtr;
			break;
		case mSCRIPT_TYPE_OBJECT:
			value = mScriptValueAlloc(derefPtr.type);
			value->value.opaque = derefPtr.value.opaque;
			weakref = mScriptContextSetWeakref(luaContext->d.context, value);
			needsWeakref = true;
			mScriptContextDisownWeakref(luaContext->d.context, weakref);
			mScriptValueDeref(value);
			break;
		default:
			return false;
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
	case mSCRIPT_TYPE_VOID:
		lua_pushnil(luaContext->lua);
		break;
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
		if (value->type == mSCRIPT_TYPE_MS_BOOL) {
			lua_pushboolean(luaContext->lua, !!value->value.u32);
		} else if (value->type->size <= 4) {
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
		if (!value->value.string) {
			lua_pushnil(luaContext->lua);
			break;
		}
		if (value->type == mSCRIPT_TYPE_MS_STR) {
			lua_pushlstring(luaContext->lua, value->value.string->buffer, value->value.string->size);
			break;
		}
		if (value->type == mSCRIPT_TYPE_MS_CHARP) {
			lua_pushstring(luaContext->lua, value->value.copaque);
			break;
		}
		ok = false;
		break;
	case mSCRIPT_TYPE_LIST:
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		if (needsWeakref) {
			*newValue = mSCRIPT_MAKE(WEAKREF, weakref);
		} else {
			mScriptValueRef(value);
			mScriptValueWrap(value, newValue);
		}
		lua_getfield(luaContext->lua, LUA_REGISTRYINDEX, "mSTList");
		lua_setmetatable(luaContext->lua, -2);
		break;
	case mSCRIPT_TYPE_TABLE:
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		if (needsWeakref) {
			*newValue = mSCRIPT_MAKE(WEAKREF, weakref);
		} else {
			mScriptValueRef(value);
			mScriptValueWrap(value, newValue);
		}
		lua_getfield(luaContext->lua, LUA_REGISTRYINDEX, "mSTTable");
		lua_setmetatable(luaContext->lua, -2);
		break;
	case mSCRIPT_TYPE_FUNCTION:
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		newValue->type = value->type;
		newValue->refs = mSCRIPT_VALUE_UNREF;
		newValue->type->alloc(newValue);
		lua_pushcclosure(luaContext->lua, _luaThunk, 1);
		break;
	case mSCRIPT_TYPE_OBJECT:
		if (!value->value.opaque) {
			lua_pushnil(luaContext->lua);
			break;
		}
		newValue = lua_newuserdata(luaContext->lua, sizeof(*newValue));
		if (needsWeakref) {
			*newValue = mSCRIPT_MAKE(WEAKREF, weakref);
		} else {
			mScriptValueRef(value);
			mScriptValueWrap(value, newValue);
		}
		lua_getfield(luaContext->lua, LUA_REGISTRYINDEX, "mSTStruct");
		lua_setmetatable(luaContext->lua, -2);
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
	char dirname[PATH_MAX];
	name[0] = '\0';
	dirname[0] = '\0';
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
				size_t len = lastSlash - filename + 1;
				if (sizeof(dirname) < len) {
					len = sizeof(dirname);
				}
				strlcpy(dirname, filename, len);
			}
			snprintf(name, sizeof(name), "@%s", filename);
		}
		filename = name;
	}
#if LUA_VERSION_NUM >= 502
	int ret = lua_load(luaContext->lua, _reader, &data, filename, "t");
#else
	int ret = lua_load(luaContext->lua, _reader, &data, filename);
#endif
	switch (ret) {
	case LUA_OK:
		// Create new _ENV
		lua_newtable(luaContext->lua);

		// Make the old _ENV the __index in the metatable
		lua_newtable(luaContext->lua);
		lua_pushliteral(luaContext->lua, "__index");
		lua_getupvalue(luaContext->lua, -4, 1);
		lua_rawset(luaContext->lua, -3);

		lua_pushliteral(luaContext->lua, "__newindex");
		lua_getupvalue(luaContext->lua, -4, 1);
		lua_rawset(luaContext->lua, -3);

		lua_setmetatable(luaContext->lua, -2);

		lua_pushliteral(luaContext->lua, "script");
		lua_newtable(luaContext->lua);

		if (dirname[0]) {
			lua_pushliteral(luaContext->lua, "require");
			lua_pushstring(luaContext->lua, dirname);
			lua_pushcclosure(luaContext->lua, _luaRequireShim, 1);
			lua_rawset(luaContext->lua, -5);

			lua_pushliteral(luaContext->lua, "dir");
			lua_pushstring(luaContext->lua, dirname);
			lua_rawset(luaContext->lua, -3);
		}

		if (name[0] == '@') {
			lua_pushliteral(luaContext->lua, "path");
			lua_pushstring(luaContext->lua, &name[1]);
			lua_rawset(luaContext->lua, -3);
		}

		lua_rawset(luaContext->lua, -3);
		lua_setupvalue(luaContext->lua, -2, 1);
		luaContext->func = luaL_ref(luaContext->lua, LUA_REGISTRYINDEX);
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

	lua_rawgeti(luaContext->lua, LUA_REGISTRYINDEX, luaContext->func);
	return _luaInvoke(luaContext, NULL);
}

const char* _luaGetError(struct mScriptEngineContext* context) {
	struct mScriptEngineContextLua* luaContext = (struct mScriptEngineContextLua*) context;
	return luaContext->lastError;
}

bool _luaPushFrame(struct mScriptEngineContextLua* luaContext, struct mScriptList* frame) {
	bool ok = true;
	if (frame) {
		size_t i;
		for (i = 0; i < mScriptListSize(frame); ++i) {
			struct mScriptValue* value = mScriptListGetPointer(frame, i);
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
			struct mScriptValue* tail = mScriptListAppend(frame);
			mScriptValueWrap(value, tail);
			if (tail->type == value->type) {
				mScriptValueDeref(value);
			}
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

void _freeFrame(struct mScriptList* frame) {
	size_t i;
	for (i = 0; i < mScriptListSize(frame); ++i) {
		struct mScriptValue* val = mScriptValueUnwrap(mScriptListGetPointer(frame, i));
		if (val) {
			mScriptValueDeref(val);
		}
	}
}

void _autofreeFrame(struct mScriptContext* context, struct mScriptList* frame) {
	size_t i;
	for (i = 0; i < mScriptListSize(frame); ++i) {
		struct mScriptValue* val = mScriptValueUnwrap(mScriptListGetPointer(frame, i));
		if (val) {
			mScriptContextFillPool(context, val);
		}
	}
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

	if (frame && !_luaPushFrame(luaContext, &frame->arguments)) {
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
	lua_rawget(lua, LUA_REGISTRYINDEX);
	if (lua_type(lua, -1) != LUA_TLIGHTUSERDATA) {
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
		_freeFrame(&frame.arguments);
		mScriptContextDrainPool(luaContext->d.context);
		mScriptFrameDeinit(&frame);
		luaL_traceback(lua, lua, "Error calling function (translating arguments into runtime)", 1);
		return lua_error(lua);
	}

	struct mScriptValue* fn = lua_touserdata(lua, lua_upvalueindex(1));
	_autofreeFrame(luaContext->d.context, &frame.arguments);
	if (!fn || !mScriptInvoke(fn, &frame)) {
		mScriptContextDrainPool(luaContext->d.context);
		mScriptFrameDeinit(&frame);
		luaL_traceback(lua, lua, "Error calling function (invoking failed)", 1);
		return lua_error(lua);
	}

	bool ok = _luaPushFrame(luaContext, &frame.returnValues);
	mScriptContextDrainPool(luaContext->d.context);
	mScriptFrameDeinit(&frame);
	if (!ok) {
		luaL_traceback(lua, lua, "Error calling function (translating return values from runtime)", 1);
		return lua_error(lua);
	}

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
		return lua_error(lua);
	}
	strlcpy(key, keyPtr, sizeof(key));
	lua_pop(lua, 2);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (!obj) {
		luaL_traceback(lua, lua, "Invalid object", 1);
		return lua_error(lua);
	}

	if (!mScriptObjectGet(obj, key, &val)) {
		char error[MAX_KEY_SIZE + 16];
		snprintf(error, sizeof(error), "Invalid key '%s'", key);
		luaL_traceback(lua, lua, "Invalid key", 1);
		return lua_error(lua);
	}

	if (!_luaWrap(luaContext, &val)) {
		luaL_traceback(lua, lua, "Error translating value from runtime", 1);
		return lua_error(lua);
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
		return lua_error(lua);
	}
	strlcpy(key, keyPtr, sizeof(key));
	lua_pop(lua, 2);

	if (!val) {
		luaL_traceback(lua, lua, "Error translating value to runtime", 1);
		return lua_error(lua);
	}

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (!obj) {
		mScriptValueDeref(val);
		mScriptContextDrainPool(luaContext->d.context);
		luaL_traceback(lua, lua, "Invalid object", 1);
		return lua_error(lua);
	}

	if (!mScriptObjectSet(obj, key, val)) {
		mScriptValueDeref(val);
		mScriptContextDrainPool(luaContext->d.context);
		char error[MAX_KEY_SIZE + 16];
		snprintf(error, sizeof(error), "Invalid key '%s'", key);
		luaL_traceback(lua, lua, "Invalid key", 1);
		return lua_error(lua);
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
	if (obj->type->base == mSCRIPT_TYPE_WRAPPER) {
		obj = mScriptValueUnwrap(obj);
	}
	if (!obj || obj->type != mSCRIPT_TYPE_MS_TABLE) {
		luaL_traceback(lua, lua, "Invalid table", 1);
		return lua_error(lua);
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
		return lua_error(lua);
	}
	return 1;
}

int _luaLenTable(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);
	struct mScriptValue* obj = lua_touserdata(lua, -1);
	lua_pop(lua, 1);

	obj = mScriptContextAccessWeakref(luaContext->d.context, obj);
	if (obj->type->base == mSCRIPT_TYPE_WRAPPER) {
		obj = mScriptValueUnwrap(obj);
	}
	if (!obj || obj->type != mSCRIPT_TYPE_MS_TABLE) {
		luaL_traceback(lua, lua, "Invalid table", 1);
		return lua_error(lua);
	}

	struct mScriptValue val = mSCRIPT_MAKE_U64(mScriptTableSize(obj));

	if (!_luaWrap(luaContext, &val)) {
		luaL_traceback(lua, lua, "Error translating value from runtime", 1);
		return lua_error(lua);
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
	if (table->type->base == mSCRIPT_TYPE_WRAPPER) {
		table = mScriptValueUnwrap(table);
	}
	if (!table || table->type != mSCRIPT_TYPE_MS_TABLE) {
		luaL_traceback(lua, lua, "Invalid table", 1);
		return lua_error(lua);
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
		return lua_error(lua);
	}

	if (!_luaWrap(luaContext, mScriptTableIteratorGetValue(table, &iter))) {
		luaL_traceback(lua, lua, "Iteration error", 1);
		return lua_error(lua);
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
		return lua_error(lua);
	}
	struct mScriptList* list = obj->value.list;

	// Lua indexes from 1
	if (index < 1) {
		luaL_traceback(lua, lua, "Invalid index", 1);
		return lua_error(lua);
	}
	if ((size_t) index > mScriptListSize(list)) {
		return 0;
	}
	--index;

	struct mScriptValue* val = mScriptListGetPointer(list, index);
	if (!_luaWrap(luaContext, val)) {
		luaL_traceback(lua, lua, "Error translating value from runtime", 1);
		return lua_error(lua);
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
		return lua_error(lua);
	}
	struct mScriptList* list = obj->value.list;
	lua_pushinteger(lua, mScriptListSize(list));
	return 1;
}

static int _luaRequireShim(lua_State* lua) {
	struct mScriptEngineContextLua* luaContext = _luaGetContext(lua);

	int oldtop = lua_gettop(luaContext->lua);
	const char* path = lua_tostring(lua, lua_upvalueindex(1));

	lua_getglobal(luaContext->lua, "package");

	lua_pushliteral(luaContext->lua, "path");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?.lua;");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?/init.lua;");
	lua_pushliteral(luaContext->lua, "path");
	lua_gettable(luaContext->lua, -7);
	char* oldpath = strdup(lua_tostring(luaContext->lua, -1));
	lua_concat(luaContext->lua, 5);
	lua_settable(luaContext->lua, -3);

#ifdef _WIN32
#define DLL "dll"
#elif defined(__APPLE__)
#define DLL "dylib"
#else
#define DLL "so"
#endif
	lua_pushliteral(luaContext->lua, "cpath");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?." DLL ";");
	lua_pushstring(luaContext->lua, path);
	lua_pushliteral(luaContext->lua, "/?/init." DLL ";");
	lua_pushliteral(luaContext->lua, "cpath");
	lua_gettable(luaContext->lua, -7);
	char* oldcpath = strdup(lua_tostring(luaContext->lua, -1));
	lua_concat(luaContext->lua, 5);
	lua_settable(luaContext->lua, -3);

	lua_pop(luaContext->lua, 1);

	lua_rawgeti(luaContext->lua, LUA_REGISTRYINDEX, luaContext->require);
	lua_insert(luaContext->lua, -2);
	int ret = lua_pcall(luaContext->lua, 1, LUA_MULTRET, 0);

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
		return lua_error(luaContext->lua);
	}

	int newtop = lua_gettop(luaContext->lua);
	return newtop - oldtop + 1;
}
