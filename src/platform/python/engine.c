/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "engine.h"

#include <mgba/core/scripting.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#ifdef USE_DEBUGGERS
#include <mgba/debugger/debugger.h>
#endif

#include "lib.h"

static const char* mPythonScriptEngineName(struct mScriptEngine*);
static bool mPythonScriptEngineInit(struct mScriptEngine*, struct mScriptBridge*);
static void mPythonScriptEngineDeinit(struct mScriptEngine*);
static bool mPythonScriptEngineIsScript(struct mScriptEngine*, const char* name, struct VFile* vf);
static bool mPythonScriptEngineLoadScript(struct mScriptEngine*, const char* name, struct VFile* vf);
static void mPythonScriptEngineRun(struct mScriptEngine*);
static bool mPythonScriptEngineLookupSymbol(struct mScriptEngine*, const char* name, int32_t* out);

#ifdef USE_DEBUGGERS
static void mPythonScriptDebuggerEntered(struct mScriptEngine*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);
#endif

struct mPythonScriptEngine {
	struct mScriptEngine d;
	struct mScriptBridge* sb;
};

struct mPythonScriptEngine* mPythonCreateScriptEngine(void) {
	struct mPythonScriptEngine* engine = malloc(sizeof(*engine));
	engine->d.name = mPythonScriptEngineName;
	engine->d.init = mPythonScriptEngineInit;
	engine->d.deinit = mPythonScriptEngineDeinit;
	engine->d.isScript = mPythonScriptEngineIsScript;
	engine->d.loadScript = mPythonScriptEngineLoadScript;
	engine->d.run = mPythonScriptEngineRun;
	engine->d.lookupSymbol = mPythonScriptEngineLookupSymbol;
#ifdef USE_DEBUGGERS
	engine->d.debuggerEntered = mPythonScriptDebuggerEntered;
#endif
	engine->sb = NULL;
	return engine;
}

void mPythonSetup(struct mScriptBridge* sb) {
	struct mPythonScriptEngine* se = mPythonCreateScriptEngine();
	mScriptBridgeInstallEngine(sb, &se->d);
}

const char* mPythonScriptEngineName(struct mScriptEngine* se) {
	UNUSED(se);
	return "python";
}

bool mPythonScriptEngineInit(struct mScriptEngine* se, struct mScriptBridge* sb) {
	struct mPythonScriptEngine* engine = (struct mPythonScriptEngine*) se;
	engine->sb = sb;
	return true;
}

void mPythonScriptEngineDeinit(struct mScriptEngine* se) {
	free(se);
}

bool mPythonScriptEngineIsScript(struct mScriptEngine* se, const char* name, struct VFile* vf) {
	UNUSED(se);
	UNUSED(vf);
	return endswith(name, ".py");
}

bool mPythonScriptEngineLoadScript(struct mScriptEngine* se, const char* name, struct VFile* vf) {
	UNUSED(se);
	return mPythonLoadScript(name, vf);
}

void mPythonScriptEngineRun(struct mScriptEngine* se) {
	struct mPythonScriptEngine* engine = (struct mPythonScriptEngine*) se;

#ifdef USE_DEBUGGERS
	struct mDebugger* debugger = mScriptBridgeGetDebugger(engine->sb);
	if (debugger) {
		mPythonSetDebugger(debugger);
	}
#endif

	mPythonRunPending();
}

bool mPythonScriptEngineLookupSymbol(struct mScriptEngine* se, const char* name, int32_t* out) {
	UNUSED(se);
	return mPythonLookupSymbol(name, out);
}

#ifdef USE_DEBUGGERS
void mPythonScriptDebuggerEntered(struct mScriptEngine* se, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct mPythonScriptEngine* engine = (struct mPythonScriptEngine*) se;

	struct mDebugger* debugger = mScriptBridgeGetDebugger(engine->sb);
	if (!debugger) {
		return;
	}

	mPythonSetDebugger(debugger);
	mPythonDebuggerEntered(reason, info);
}
#endif
