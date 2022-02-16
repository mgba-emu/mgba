/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_CONTEXT_H
#define M_SCRIPT_CONTEXT_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/script/types.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

struct mScriptFrame;
struct mScriptFunction;
struct mScriptEngineContext;

struct mScriptContext {
	struct mScriptValue rootScope;
	struct Table engines;
};

struct mScriptEngine2 {
	const char* name;

	void (*init)(struct mScriptEngine2*);
	void (*deinit)(struct mScriptEngine2*);

	struct mScriptEngineContext* (*create)(struct mScriptEngine2*, struct mScriptContext*);
};

struct mScriptEngineContext {
	struct mScriptContext* context;
	void (*destroy)(struct mScriptEngineContext*);

	bool (*setGlobal)(struct mScriptEngineContext*, const char* name, struct mScriptValue*);
	struct mScriptValue* (*getGlobal)(struct mScriptEngineContext*, const char* name);

	bool (*load)(struct mScriptEngineContext*, struct VFile*, const char** error);
	bool (*run)(struct mScriptEngineContext*);
};

void mScriptContextInit(struct mScriptContext*);
void mScriptContextDeinit(struct mScriptContext*);

void mScriptContextRegisterEngine(struct mScriptContext*, struct mScriptEngine2*);

void mScriptContextAddGlobal(struct mScriptContext*, const char* key, struct mScriptValue* value);
void mScriptContextRemoveGlobal(struct mScriptContext*, const char* key);

bool mScriptInvoke(const struct mScriptFunction* fn, struct mScriptFrame* frame);

#endif
