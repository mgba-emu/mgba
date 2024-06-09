/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPTING_H
#define M_SCRIPTING_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef ENABLE_DEBUGGERS
#include <mgba/debugger/debugger.h>
#endif
#include <mgba/script/macros.h>
#include <mgba/script/types.h>

struct mCore;
struct mScriptTextBuffer;
mSCRIPT_DECLARE_STRUCT(mCore);
mSCRIPT_DECLARE_STRUCT(mLogger);

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

#ifdef ENABLE_DEBUGGERS
	void (*debuggerEntered)(struct mScriptEngine*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
#endif
};

struct mScriptBridge* mScriptBridgeCreate(void);
void mScriptBridgeDestroy(struct mScriptBridge*);

void mScriptBridgeInstallEngine(struct mScriptBridge*, struct mScriptEngine*);

#ifdef ENABLE_DEBUGGERS
void mScriptBridgeSetDebugger(struct mScriptBridge*, struct mDebugger*);
struct mDebugger* mScriptBridgeGetDebugger(struct mScriptBridge*);
void mScriptBridgeDebuggerEntered(struct mScriptBridge*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
#endif

void mScriptBridgeRun(struct mScriptBridge*);
#ifdef ENABLE_VFS
bool mScriptBridgeLoadScript(struct mScriptBridge*, const char* name);
#endif

bool mScriptBridgeLookupSymbol(struct mScriptBridge*, const char* name, int32_t* out);

struct mScriptContext;
void mScriptContextAttachCore(struct mScriptContext*, struct mCore*);
void mScriptContextDetachCore(struct mScriptContext*);

CXX_GUARD_END

#endif
