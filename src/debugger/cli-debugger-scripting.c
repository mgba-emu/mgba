/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/scripting.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#include <mgba/debugger/debugger.h>
#include <mgba/internal/debugger/cli-debugger.h>

static const char* CLIScriptEngineName(struct mScriptEngine*);
static bool CLIScriptEngineInit(struct mScriptEngine*, struct mScriptBridge*);
static void CLIScriptEngineDeinit(struct mScriptEngine*);
static bool CLIScriptEngineIsScript(struct mScriptEngine*, const char* name, struct VFile* vf);
static bool CLIScriptEngineLoadScript(struct mScriptEngine*, const char* name, struct VFile* vf);
static void CLIScriptEngineRun(struct mScriptEngine*);
static bool CLIScriptEngineLookupSymbol(struct mScriptEngine*, const char* name, int32_t* out);
static void CLIScriptDebuggerEntered(struct mScriptEngine*, enum mDebuggerEntryReason, struct mDebuggerEntryInfo*);

struct CLIScriptStatement {
	char* command;
	size_t commandLen;
};

DECLARE_VECTOR(CLIScript, struct CLIScriptStatement);
DEFINE_VECTOR(CLIScript, struct CLIScriptStatement);

struct CLIScriptEngine {
	struct mScriptEngine d;
	struct mScriptBridge* sb;
	struct CLIScript script;
};

static void CLIScriptEngineClear(struct CLIScriptEngine* engine) {
	size_t i = CLIScriptSize(&engine->script);
	while (i-- > 0) {
		struct CLIScriptStatement* statement = CLIScriptGetPointer(&engine->script, i);
		free(statement->command);
	}
	CLIScriptClear(&engine->script);
}

struct CLIScriptEngine* CLICreateScriptEngine(void) {
	struct CLIScriptEngine* engine = malloc(sizeof(*engine));
	engine->d.name = CLIScriptEngineName;
	engine->d.init = CLIScriptEngineInit;
	engine->d.deinit = CLIScriptEngineDeinit;
	engine->d.isScript = CLIScriptEngineIsScript;
	engine->d.loadScript = CLIScriptEngineLoadScript;
	engine->d.run = CLIScriptEngineRun;
	engine->d.lookupSymbol = CLIScriptEngineLookupSymbol;
	engine->d.debuggerEntered = CLIScriptDebuggerEntered;
	engine->sb = NULL;
	return engine;
}

void CLIDebuggerScriptEngineInstall(struct mScriptBridge* sb) {
	struct CLIScriptEngine* se = CLICreateScriptEngine();
	mScriptBridgeInstallEngine(sb, &se->d);
}

const char* CLIScriptEngineName(struct mScriptEngine* se) {
	UNUSED(se);
	return "cli-debugger";
}

bool CLIScriptEngineInit(struct mScriptEngine* se, struct mScriptBridge* sb) {
	struct CLIScriptEngine* engine = (struct CLIScriptEngine*) se;
	engine->sb = sb;
	CLIScriptInit(&engine->script, 0);
	return true;
}

void CLIScriptEngineDeinit(struct mScriptEngine* se) {
	struct CLIScriptEngine* engine = (struct CLIScriptEngine*) se;
	CLIScriptEngineClear(engine);
	CLIScriptDeinit(&engine->script);
	free(se);
}

bool CLIScriptEngineIsScript(struct mScriptEngine* se, const char* name, struct VFile* vf) {
	UNUSED(se);
	UNUSED(vf);
	return endswith(name, ".mrc");
}

bool CLIScriptEngineLoadScript(struct mScriptEngine* se, const char* name, struct VFile* vf) {
	UNUSED(name);
	struct CLIScriptEngine* engine = (struct CLIScriptEngine*) se;
	char buffer[256];
	ssize_t size;
	CLIScriptEngineClear(engine);
	struct CLIScriptStatement* statement;
	while ((size = vf->readline(vf, buffer, sizeof(buffer))) > 0) {
		if (buffer[size - 1] == '\n') {
			--size;
		}
		statement = CLIScriptAppend(&engine->script);
		statement->command = strndup(buffer, size);
		statement->commandLen = size;
	}
	return true;
}

void CLIScriptEngineRun(struct mScriptEngine* se) {
	struct CLIScriptEngine* engine = (struct CLIScriptEngine*) se;
	struct CLIDebugger* debugger = (struct CLIDebugger*) mScriptBridgeGetDebugger(engine->sb);
	struct CLIScriptStatement* statement;
	size_t statementCount = CLIScriptSize(&engine->script);
	size_t i;
	for (i = 0; i < statementCount; i++) {
		statement = CLIScriptGetPointer(&engine->script, i);
		CLIDebuggerRunCommand(debugger, statement->command, statement->commandLen);
	}
}

bool CLIScriptEngineLookupSymbol(struct mScriptEngine* se, const char* name, int32_t* out) {
	UNUSED(se);
	UNUSED(name);
	UNUSED(out);
	return false;
}

void CLIScriptDebuggerEntered(struct mScriptEngine* se, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	UNUSED(reason);
	UNUSED(info);
	struct CLIScriptEngine* engine = (struct CLIScriptEngine*) se;

	struct mDebugger* debugger = mScriptBridgeGetDebugger(engine->sb);
	if (!debugger) {
		return;
	}

	// TODO: CLIDebuggerEntered(reason, info);
}
