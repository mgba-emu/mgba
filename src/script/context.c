/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/context.h>

struct mScriptKVPair {
	const char* key;
	struct mScriptValue* value;
};

static void _engineContextDestroy(void* ctx) {
	struct mScriptEngineContext* context = ctx;
	context->destroy(context);
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

void mScriptContextInit(struct mScriptContext* context) {
	HashTableInit(&context->rootScope, 0, (void (*)(void*)) mScriptValueDeref);
	HashTableInit(&context->engines, 0, _engineContextDestroy);
}

void mScriptContextDeinit(struct mScriptContext* context) {
	HashTableDeinit(&context->engines);
	HashTableDeinit(&context->rootScope);
}

struct mScriptEngineContext* mScriptContextRegisterEngine(struct mScriptContext* context, struct mScriptEngine2* engine) {
	struct mScriptEngineContext* ectx = engine->create(engine, context);
	if (ectx) {
		HashTableInsert(&context->engines, engine->name, ectx);
	}
	return ectx;
}

void mScriptContextSetGlobal(struct mScriptContext* context, const char* key, struct mScriptValue* value) {
	mScriptValueRef(value);
	HashTableInsert(&context->rootScope, key, value);
	struct mScriptKVPair pair = {
		.key = key,
		.value = value
	};
	HashTableEnumerate(&context->engines, _contextAddGlobal, &pair);
}

void mScriptContextRemoveGlobal(struct mScriptContext* context, const char* key) {
	if (!HashTableLookup(&context->rootScope, key)) {
		return;
	}
	// Since _contextRemoveGlobal doesn't mutate |key|, this cast should be safe
	HashTableEnumerate(&context->engines, _contextRemoveGlobal, (char*) key);
	HashTableRemove(&context->rootScope, key);
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
