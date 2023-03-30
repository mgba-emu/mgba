/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_CONTEXT_H
#define M_SCRIPT_CONTEXT_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/script/types.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

#define mSCRIPT_KV_PAIR(KEY, VALUE) { #KEY, VALUE }
#define mSCRIPT_CONSTANT_PAIR(NS, CONST) { #CONST, mScriptValueCreateFromSInt(NS ## _ ## CONST) }
#define mSCRIPT_KV_SENTINEL { NULL, NULL }

mLOG_DECLARE_CATEGORY(SCRIPT);

struct mScriptFrame;
struct mScriptFunction;
struct mScriptEngineContext;

struct mScriptContext {
	struct Table rootScope;
	struct Table engines;
	struct mScriptList refPool;
	struct Table weakrefs;
	uint32_t nextWeakref;
	struct Table callbacks;
	struct Table callbackId;
	uint32_t nextCallbackId;
	struct mScriptValue* constants;
	struct Table docstrings;
};

struct mScriptEngine2 {
	const char* name;

	void (*init)(struct mScriptEngine2*);
	void (*deinit)(struct mScriptEngine2*);

	struct mScriptEngineContext* (*create)(struct mScriptEngine2*, struct mScriptContext*);
};

struct mScriptEngineContext {
	struct mScriptContext* context;
	struct mScriptEngine2* engine;

	void (*destroy)(struct mScriptEngineContext*);

	bool (*isScript)(struct mScriptEngineContext*, const char* name, struct VFile* vf);

	bool (*setGlobal)(struct mScriptEngineContext*, const char* name, struct mScriptValue*);
	struct mScriptValue* (*getGlobal)(struct mScriptEngineContext*, const char* name);
	struct mScriptValue* (*rootScope)(struct mScriptEngineContext*);

	bool (*load)(struct mScriptEngineContext*, const char* filename, struct VFile*);
	bool (*run)(struct mScriptEngineContext*);
	const char* (*getError)(struct mScriptEngineContext*);

	struct Table docroot;
};

struct mScriptKVPair {
	const char* key;
	struct mScriptValue* value;
};

void mScriptContextInit(struct mScriptContext*);
void mScriptContextDeinit(struct mScriptContext*);

void mScriptContextFillPool(struct mScriptContext*, struct mScriptValue*);
void mScriptContextDrainPool(struct mScriptContext*);

struct mScriptEngineContext* mScriptContextRegisterEngine(struct mScriptContext*, struct mScriptEngine2*);
void mScriptContextRegisterEngines(struct mScriptContext*);

void mScriptContextSetGlobal(struct mScriptContext*, const char* key, struct mScriptValue* value);
struct mScriptValue* mScriptContextGetGlobal(struct mScriptContext*, const char* key);
void mScriptContextRemoveGlobal(struct mScriptContext*, const char* key);
struct mScriptValue* mScriptContextEnsureGlobal(struct mScriptContext*, const char* key, const struct mScriptType* type);

uint32_t mScriptContextSetWeakref(struct mScriptContext*, struct mScriptValue* value);
struct mScriptValue* mScriptContextMakeWeakref(struct mScriptContext*, struct mScriptValue* value);
struct mScriptValue* mScriptContextAccessWeakref(struct mScriptContext*, struct mScriptValue* value);
void mScriptContextClearWeakref(struct mScriptContext*, uint32_t weakref);
void mScriptContextDisownWeakref(struct mScriptContext*, uint32_t weakref);

void mScriptContextExportConstants(struct mScriptContext* context, const char* nspace, struct mScriptKVPair* constants);
void mScriptContextExportNamespace(struct mScriptContext* context, const char* nspace, struct mScriptKVPair* value);

void mScriptContextTriggerCallback(struct mScriptContext*, const char* callback, struct mScriptList* args);
uint32_t mScriptContextAddCallback(struct mScriptContext*, const char* callback, struct mScriptValue* value);
uint32_t mScriptContextAddOneshot(struct mScriptContext*, const char* callback, struct mScriptValue* value);
void mScriptContextRemoveCallback(struct mScriptContext*, uint32_t cbid);

void mScriptContextSetDocstring(struct mScriptContext*, const char* key, const char* docstring);
const char* mScriptContextGetDocstring(struct mScriptContext*, const char* key);

void mScriptEngineExportDocNamespace(struct mScriptEngineContext*, const char* nspace, struct mScriptKVPair* value);
void mScriptEngineSetDocstring(struct mScriptEngineContext*, const char* key, const char* docstring);
const char* mScriptEngineGetDocstring(struct mScriptEngineContext*, const char* key);

struct VFile;
bool mScriptContextLoadVF(struct mScriptContext*, const char* name, struct VFile* vf);
bool mScriptContextLoadFile(struct mScriptContext*, const char* path);

bool mScriptInvoke(const struct mScriptValue* fn, struct mScriptFrame* frame);

CXX_GUARD_END

#endif
