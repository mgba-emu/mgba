/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/context.h>
#ifdef USE_LUA
#include <mgba/internal/script/lua.h>
#endif

#define KEY_NAME_MAX 128

struct mScriptFileInfo {
	const char* name;
	struct VFile* vf;
	struct mScriptEngineContext* context;
};

struct mScriptCallbackInfo {
	struct mScriptValue* fn;
	const char* callback;
	uint32_t id;
	bool oneshot;
};

static void _engineContextDestroy(void* ctx) {
	struct mScriptEngineContext* context = ctx;
	context->destroy(context);
}

static void _engineAddGlobal(const char* key, void* value, void* user) {
	struct mScriptEngineContext* context = user;
	context->setGlobal(context, key, value);
}

static void _contextAddGlobal(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngineContext* context = value;
	struct mScriptKVPair* pair = user;
	context->setGlobal(context, pair->key, pair->value);
}

static void _contextRemoveGlobal(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngineContext* context = value;
	context->setGlobal(context, user, NULL);
}

static void _contextFindForFile(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptFileInfo* info = user;
	struct mScriptEngineContext* context = value;
	if (info->context) {
		return;
	}
	if (context->isScript(context, info->name, info->vf)) {
		info->context = context;
	}
}

static void _freeTable(void* data) {
	struct Table* table = data;

	struct TableIterator iter;
	if (TableIteratorStart(table, &iter)) {
		do {
			struct mScriptCallbackInfo* info = TableIteratorGetValue(table, &iter);
			mScriptValueDeref(info->fn);
		} while (TableIteratorNext(table, &iter));
	}

	TableDeinit(table);
	free(table);
}

void mScriptContextInit(struct mScriptContext* context) {
	HashTableInit(&context->rootScope, 0, (void (*)(void*)) mScriptValueDeref);
	HashTableInit(&context->engines, 0, _engineContextDestroy);
	mScriptListInit(&context->refPool, 0);
	TableInit(&context->weakrefs, 0, (void (*)(void*)) mScriptValueDeref);
	context->nextWeakref = 1;
	HashTableInit(&context->callbacks, 0, _freeTable);
	TableInit(&context->callbackId, 0, free);
	context->nextCallbackId = 1;
	context->constants = NULL;
	HashTableInit(&context->docstrings, 0, NULL);
}

void mScriptContextDeinit(struct mScriptContext* context) {
	HashTableDeinit(&context->rootScope);
	mScriptContextDrainPool(context);
	HashTableDeinit(&context->weakrefs);
	mScriptListDeinit(&context->refPool);
	HashTableDeinit(&context->callbacks);
	TableDeinit(&context->callbackId);
	HashTableDeinit(&context->engines);
	HashTableDeinit(&context->docstrings);
}

void mScriptContextFillPool(struct mScriptContext* context, struct mScriptValue* value) {
	if (value->refs == mSCRIPT_VALUE_UNREF) {
		return;
	}

	struct mScriptValue* poolEntry = mScriptListAppend(&context->refPool);
	poolEntry->type = mSCRIPT_TYPE_MS_WRAPPER;
	poolEntry->value.opaque = value;
	poolEntry->refs = mSCRIPT_VALUE_UNREF;
}

void mScriptContextDrainPool(struct mScriptContext* context) {
	size_t i;
	for (i = 0; i < mScriptListSize(&context->refPool); ++i) {
		struct mScriptValue* value = mScriptListGetPointer(&context->refPool, i);
		if (value->type->base == mSCRIPT_TYPE_WRAPPER) {
			value = mScriptValueUnwrap(value);
			mScriptValueDeref(value);
		} else if (value->type == mSCRIPT_TYPE_MS_WEAKREF) {
			mScriptContextClearWeakref(context, value->value.u32);
		}
	}
	mScriptListClear(&context->refPool);
}

struct mScriptEngineContext* mScriptContextRegisterEngine(struct mScriptContext* context, struct mScriptEngine2* engine) {
	struct mScriptEngineContext* ectx = engine->create(engine, context);
	if (ectx) {
		HashTableInsert(&context->engines, engine->name, ectx);
		HashTableEnumerate(&context->rootScope, _engineAddGlobal, ectx);
	}
	return ectx;
}

void mScriptContextRegisterEngines(struct mScriptContext* context) {
	UNUSED(context);
#ifdef USE_LUA
	mScriptContextRegisterEngine(context, mSCRIPT_ENGINE_LUA);
#endif
}

void mScriptContextSetGlobal(struct mScriptContext* context, const char* key, struct mScriptValue* value) {
	struct mScriptValue* oldValue = HashTableLookup(&context->rootScope, key);
	if (oldValue) {
		mScriptContextClearWeakref(context, oldValue->value.u32);
	}
	value = mScriptContextMakeWeakref(context, value);
	HashTableInsert(&context->rootScope, key, value);
	struct mScriptKVPair pair = {
		.key = key,
		.value = value
	};
	HashTableEnumerate(&context->engines, _contextAddGlobal, &pair);
}

struct mScriptValue* mScriptContextGetGlobal(struct mScriptContext* context, const char* key) {
	struct mScriptValue* weakref = HashTableLookup(&context->rootScope, key);
	if (!weakref) {
		return NULL;
	}
	return mScriptContextAccessWeakref(context, weakref);
}

void mScriptContextRemoveGlobal(struct mScriptContext* context, const char* key) {
	if (!HashTableLookup(&context->rootScope, key)) {
		return;
	}
	// Since _contextRemoveGlobal doesn't mutate |key|, this cast should be safe
	HashTableEnumerate(&context->engines, _contextRemoveGlobal, (char*) key);
	struct mScriptValue* oldValue = HashTableLookup(&context->rootScope, key);
	if (oldValue) {
		mScriptContextClearWeakref(context, oldValue->value.u32);
		HashTableRemove(&context->rootScope, key);
	}
}

struct mScriptValue* mScriptContextEnsureGlobal(struct mScriptContext* context, const char* key, const struct mScriptType* type) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, key);
	if (!value) {
		mScriptContextSetGlobal(context, key, mScriptValueAlloc(type));
		value = mScriptContextGetGlobal(context, key);
	}
	return value;
}

uint32_t mScriptContextSetWeakref(struct mScriptContext* context, struct mScriptValue* value) {
	mScriptValueRef(value);
	TableInsert(&context->weakrefs, context->nextWeakref, value);

	uint32_t nextWeakref = context->nextWeakref;
	++context->nextWeakref;
	while (TableLookup(&context->weakrefs, context->nextWeakref)) {
		++context->nextWeakref;
	}
	return nextWeakref;
}

struct mScriptValue* mScriptContextMakeWeakref(struct mScriptContext* context, struct mScriptValue* value) {
	uint32_t weakref = mScriptContextSetWeakref(context, value);
	mScriptValueDeref(value);
	value = mScriptValueAlloc(mSCRIPT_TYPE_MS_WEAKREF);
	value->value.u32 = weakref;
	return value;
}

struct mScriptValue* mScriptContextAccessWeakref(struct mScriptContext* context, struct mScriptValue* value) {
	if (value->type != mSCRIPT_TYPE_MS_WEAKREF) {
		return value;
	}
	return TableLookup(&context->weakrefs, value->value.u32);
}

void mScriptContextClearWeakref(struct mScriptContext* context, uint32_t weakref) {
	TableRemove(&context->weakrefs, weakref);
}

void mScriptContextDisownWeakref(struct mScriptContext* context, uint32_t weakref) {
	struct mScriptValue* poolEntry = mScriptListAppend(&context->refPool);
	poolEntry->type = mSCRIPT_TYPE_MS_WEAKREF;
	poolEntry->value.u32 = weakref;
	poolEntry->refs = mSCRIPT_VALUE_UNREF;
}

void mScriptContextTriggerCallback(struct mScriptContext* context, const char* callback, struct mScriptList* args) {
	struct Table* table = HashTableLookup(&context->callbacks, callback);
	if (!table) {
		return;
	}
	struct TableIterator iter;
	if (!TableIteratorStart(table, &iter)) {
		return;
	}

	struct UInt32List oneshots;
	UInt32ListInit(&oneshots, 0);
	do {
		struct mScriptFrame frame;
		struct mScriptCallbackInfo* info = TableIteratorGetValue(table, &iter);
		mScriptFrameInit(&frame);
		if (args) {
			mScriptListCopy(&frame.arguments, args);
		}
		mScriptInvoke(info->fn, &frame);
		mScriptFrameDeinit(&frame);

		if (info->oneshot) {
			*UInt32ListAppend(&oneshots) = info->id;
		}
	} while (TableIteratorNext(table, &iter));

	size_t i;
	for (i = 0; i < UInt32ListSize(&oneshots); ++i) {
		mScriptContextRemoveCallback(context, *UInt32ListGetPointer(&oneshots, i));
	}
	UInt32ListDeinit(&oneshots);
}

static uint32_t mScriptContextAddCallbackInternal(struct mScriptContext* context, const char* callback, struct mScriptValue* fn, bool oneshot) {
	if (fn->type->base != mSCRIPT_TYPE_FUNCTION) {
		return 0;
	}
	struct Table* table = HashTableLookup(&context->callbacks, callback);
	if (!table) {
		table = calloc(1, sizeof(*table));
		TableInit(table, 0, NULL);
		HashTableInsert(&context->callbacks, callback, table);
	}
	struct mScriptCallbackInfo* info = malloc(sizeof(*info));
	// Steal the string from the table key, since it's guaranteed to outlive this struct
	struct TableIterator iter;
	HashTableIteratorLookup(&context->callbacks, &iter, callback);
	info->callback = HashTableIteratorGetKey(&context->callbacks, &iter);
	info->oneshot = oneshot;
	if (fn->type->base == mSCRIPT_TYPE_WRAPPER) {
		fn = mScriptValueUnwrap(fn);
	}
	info->fn = fn;
	mScriptValueRef(fn);
	while (true) {
		uint32_t id = context->nextCallbackId;
		++context->nextCallbackId;
		if (TableLookup(&context->callbackId, id)) {
			continue;
		}
		TableInsert(&context->callbackId, id, info);
		info->id = id;
		break;
	}
	TableInsert(table, info->id, info);
	return info->id;
}

uint32_t mScriptContextAddCallback(struct mScriptContext* context, const char* callback, struct mScriptValue* fn) {
	return mScriptContextAddCallbackInternal(context, callback, fn, false);
}

uint32_t mScriptContextAddOneshot(struct mScriptContext* context, const char* callback, struct mScriptValue* fn) {
	return mScriptContextAddCallbackInternal(context, callback, fn, true);
}

void mScriptContextRemoveCallback(struct mScriptContext* context, uint32_t cbid) {
	struct mScriptCallbackInfo* info = TableLookup(&context->callbackId, cbid);
	if (!info) {
		return;
	}
	struct Table* table = HashTableLookup(&context->callbacks, info->callback);
	if (!table) {
		return;
	}
	mScriptValueDeref(info->fn);
	TableRemove(table, cbid);
	TableRemove(&context->callbackId, cbid);
}

void mScriptContextExportConstants(struct mScriptContext* context, const char* nspace, struct mScriptKVPair* constants) {
	if (!context->constants) {
		context->constants = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	}
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	size_t i;
	for (i = 0; constants[i].key; ++i) {
		struct mScriptValue* key = mScriptStringCreateFromUTF8(constants[i].key);
		mScriptTableInsert(table, key, constants[i].value);
		mScriptValueDeref(key);
		mScriptValueDeref(constants[i].value);
	}
	struct mScriptValue* key = mScriptStringCreateFromUTF8(nspace);
	mScriptTableInsert(context->constants, key, table);
	mScriptValueDeref(key);
	mScriptValueDeref(table);
}

void mScriptContextExportNamespace(struct mScriptContext* context, const char* nspace, struct mScriptKVPair* values) {
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	size_t i;
	for (i = 0; values[i].key; ++i) {
		struct mScriptValue* key = mScriptStringCreateFromUTF8(values[i].key);
		mScriptTableInsert(table, key, values[i].value);
		mScriptValueDeref(key);
		mScriptValueDeref(values[i].value);
	}
	mScriptContextSetGlobal(context, nspace, table);
}

void mScriptContextSetDocstring(struct mScriptContext* context, const char* key, const char* docstring) {
	HashTableInsert(&context->docstrings, key, (char*) docstring);
}

const char* mScriptContextGetDocstring(struct mScriptContext* context, const char* key) {
	return HashTableLookup(&context->docstrings, key);
}

void mScriptEngineExportDocNamespace(struct mScriptEngineContext* ctx, const char* nspace, struct mScriptKVPair* values) {
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	size_t i;
	for (i = 0; values[i].key; ++i) {
		struct mScriptValue* key = mScriptStringCreateFromUTF8(values[i].key);
		mScriptTableInsert(table, key, values[i].value);
		mScriptValueDeref(key);
		mScriptValueDeref(values[i].value);
	}
	HashTableInsert(&ctx->docroot, nspace, table);
}

void mScriptEngineSetDocstring(struct mScriptEngineContext* ctx, const char* key, const char* docstring) {
	char scopedKey[KEY_NAME_MAX];
	snprintf(scopedKey, sizeof(scopedKey), "%s::%s", ctx->engine->name, key);
	HashTableInsert(&ctx->context->docstrings, scopedKey, (char*) docstring);
}

const char* mScriptEngineGetDocstring(struct mScriptEngineContext* ctx, const char* key) {
	char scopedKey[KEY_NAME_MAX];
	snprintf(scopedKey, sizeof(scopedKey), "%s::%s", ctx->engine->name, key);
	return HashTableLookup(&ctx->context->docstrings, scopedKey);
}

bool mScriptContextLoadVF(struct mScriptContext* context, const char* name, struct VFile* vf) {
	struct mScriptFileInfo info = {
		.name = name,
		.vf = vf,
		.context = NULL
	};
	HashTableEnumerate(&context->engines, _contextFindForFile, &info);
	if (!info.context) {
		return false;
	}
	return info.context->load(info.context, name, vf);
}

bool mScriptContextLoadFile(struct mScriptContext* context, const char* path) {
	struct VFile* vf = VFileOpen(path, O_RDONLY);
	if (!vf) {
		return false;
	}
	bool ret = mScriptContextLoadVF(context, path, vf);
	vf->close(vf);
	return ret;
}

bool mScriptInvoke(const struct mScriptValue* val, struct mScriptFrame* frame) {
	if (val->type->base != mSCRIPT_TYPE_FUNCTION) {
		return false;
	}
	const struct mScriptTypeFunction* signature = &val->type->details.function;
	if (!mScriptCoerceFrame(&signature->parameters, &frame->arguments)) {
		return false;
	}
	const struct mScriptFunction* fn = val->value.opaque;
	return fn->call(frame, fn->context);
}
