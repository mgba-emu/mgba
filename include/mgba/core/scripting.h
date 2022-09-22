/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPTING_H
#define M_SCRIPTING_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef USE_DEBUGGERS
#include <mgba/debugger/debugger.h>
#endif
#include <mgba/script/macros.h>
#include <mgba/script/types.h>

struct mCore;
struct mScriptTextBuffer;
mSCRIPT_DECLARE_STRUCT(mCore);
mSCRIPT_DECLARE_STRUCT(mLogger);
mSCRIPT_DECLARE_STRUCT(mScriptConsole);
mSCRIPT_DECLARE_STRUCT(mScriptTextBuffer);

struct mScriptBridge;
struct VFile;
struct mScriptEngine {
	const char* (*name)(struct mScriptEngine*);

	bool (*init)(struct mScriptEngine*, struct mScriptBridge*);
	void (*deinit)(struct mScriptEngine*);
	bool (*isScript)(struct mScriptEngine*, const char* name, struct VFile* vf);
	bool (*loadScript)(struct mScriptEngine*, const char* name, struct VFile* vf);
	void (*run)(struct mScriptEngine*);
	bool (*lookupSymbol)(struct mScriptEngine*, const char* name, int32_t* out);

#ifdef USE_DEBUGGERS
	void (*debuggerEntered)(struct mScriptEngine*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
#endif
};

struct mScriptTextBuffer {
	void (*init)(struct mScriptTextBuffer*, const char* name);
	void (*deinit)(struct mScriptTextBuffer*);

	void (*setName)(struct mScriptTextBuffer*, const char* text);

	uint32_t (*getX)(const struct mScriptTextBuffer*);
	uint32_t (*getY)(const struct mScriptTextBuffer*);
	uint32_t (*cols)(const struct mScriptTextBuffer*);
	uint32_t (*rows)(const struct mScriptTextBuffer*);

	void (*print)(struct mScriptTextBuffer*, const char* text);
	void (*clear)(struct mScriptTextBuffer*);
	void (*setSize)(struct mScriptTextBuffer*, uint32_t cols, uint32_t rows);
	void (*moveCursor)(struct mScriptTextBuffer*, uint32_t x, uint32_t y);
	void (*advance)(struct mScriptTextBuffer*, int32_t);
};

struct mScriptBridge* mScriptBridgeCreate(void);
void mScriptBridgeDestroy(struct mScriptBridge*);

void mScriptBridgeInstallEngine(struct mScriptBridge*, struct mScriptEngine*);

#ifdef USE_DEBUGGERS
void mScriptBridgeSetDebugger(struct mScriptBridge*, struct mDebugger*);
struct mDebugger* mScriptBridgeGetDebugger(struct mScriptBridge*);
void mScriptBridgeDebuggerEntered(struct mScriptBridge*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
#endif

void mScriptBridgeRun(struct mScriptBridge*);
bool mScriptBridgeLoadScript(struct mScriptBridge*, const char* name);

bool mScriptBridgeLookupSymbol(struct mScriptBridge*, const char* name, int32_t* out);

struct mScriptContext;
void mScriptContextAttachCore(struct mScriptContext*, struct mCore*);
void mScriptContextDetachCore(struct mScriptContext*);

struct mLogger;
void mScriptContextAttachLogger(struct mScriptContext*, struct mLogger*);
void mScriptContextDetachLogger(struct mScriptContext*);

typedef struct mScriptTextBuffer* (*mScriptContextBufferFactory)(void*);
void mScriptContextSetTextBufferFactory(struct mScriptContext*, mScriptContextBufferFactory factory, void* cbContext);

CXX_GUARD_END

#endif
