/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/debugger/debugger.h>

#include <mgba/core/core.h>

#include <mgba/internal/debugger/cli-debugger.h>
#include <mgba/internal/debugger/symbols.h>

#ifdef USE_GDB_STUB
#include <mgba/internal/debugger/gdb-stub.h>
#endif

#ifdef ENABLE_SCRIPTING
#include <mgba/core/scripting.h>
#endif

const uint32_t DEBUGGER_ID = 0xDEADBEEF;

mLOG_DEFINE_CATEGORY(DEBUGGER, "Debugger", "core.debugger");

DEFINE_VECTOR(mBreakpointList, struct mBreakpoint);
DEFINE_VECTOR(mWatchpointList, struct mWatchpoint);

static void mDebuggerInit(void* cpu, struct mCPUComponent* component);
static void mDebuggerDeinit(struct mCPUComponent* component);

struct mDebugger* mDebuggerCreate(enum mDebuggerType type, struct mCore* core) {
	if (!core->supportsDebuggerType(core, type)) {
		return NULL;
	}

	union DebugUnion {
		struct mDebugger d;
		struct CLIDebugger cli;
#ifdef USE_GDB_STUB
		struct GDBStub gdb;
#endif
	};

	union DebugUnion* debugger = malloc(sizeof(union DebugUnion));
	memset(debugger, 0, sizeof(*debugger));

	switch (type) {
	case DEBUGGER_CLI:
		CLIDebuggerCreate(&debugger->cli);
		struct CLIDebuggerSystem* sys = core->cliDebuggerSystem(core);
		CLIDebuggerAttachSystem(&debugger->cli, sys);
		break;
	case DEBUGGER_GDB:
#ifdef USE_GDB_STUB
		GDBStubCreate(&debugger->gdb);
		GDBStubListen(&debugger->gdb, 2345, 0, GDB_WATCHPOINT_STANDARD_LOGIC);
		break;
#endif
	case DEBUGGER_NONE:
	case DEBUGGER_CUSTOM:
	case DEBUGGER_MAX:
		free(debugger);
		return 0;
		break;
	}

	return &debugger->d;
}

void mDebuggerAttach(struct mDebugger* debugger, struct mCore* core) {
	debugger->d.id = DEBUGGER_ID;
	debugger->d.init = mDebuggerInit;
	debugger->d.deinit = mDebuggerDeinit;
	debugger->core = core;
	if (!debugger->core->symbolTable) {
		debugger->core->loadSymbols(debugger->core, NULL);
	}
	debugger->platform = core->debuggerPlatform(core);
	debugger->platform->p = debugger;
	core->attachDebugger(core, debugger);
}

void mDebuggerRun(struct mDebugger* debugger) {
	switch (debugger->state) {
	case DEBUGGER_RUNNING:
		if (!debugger->platform->hasBreakpoints(debugger->platform)) {
			debugger->core->runLoop(debugger->core);
		} else {
			debugger->core->step(debugger->core);
			debugger->platform->checkBreakpoints(debugger->platform);
		}
		break;
	case DEBUGGER_CALLBACK:
		debugger->core->step(debugger->core);
		debugger->platform->checkBreakpoints(debugger->platform);
		debugger->custom(debugger);
		break;
	case DEBUGGER_PAUSED:
		if (debugger->paused) {
			debugger->paused(debugger);
		} else {
			debugger->state = DEBUGGER_RUNNING;
		}
		break;
	case DEBUGGER_SHUTDOWN:
		return;
	}
}

void mDebuggerRunFrame(struct mDebugger* debugger) {
	uint32_t frame = debugger->core->frameCounter(debugger->core);
	do {
		mDebuggerRun(debugger);
	} while (debugger->core->frameCounter(debugger->core) == frame);
}

void mDebuggerEnter(struct mDebugger* debugger, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	debugger->state = DEBUGGER_PAUSED;
	if (debugger->platform->entered) {
		debugger->platform->entered(debugger->platform, reason, info);
	}
#ifdef ENABLE_SCRIPTING
	if (debugger->bridge) {
		mScriptBridgeDebuggerEntered(debugger->bridge, reason, info);
	}
#endif
}

static void mDebuggerInit(void* cpu, struct mCPUComponent* component) {
	struct mDebugger* debugger = (struct mDebugger*) component;
	debugger->state = DEBUGGER_RUNNING;
	debugger->platform->init(cpu, debugger->platform);
	if (debugger->init) {
		debugger->init(debugger);
	}
}

static void mDebuggerDeinit(struct mCPUComponent* component) {
	struct mDebugger* debugger = (struct mDebugger*) component;
	if (debugger->deinit) {
		debugger->deinit(debugger);
	}
	debugger->platform->deinit(debugger->platform);
}

bool mDebuggerLookupIdentifier(struct mDebugger* debugger, const char* name, int32_t* value, int* segment) {
	*segment = -1;
#ifdef ENABLE_SCRIPTING
	if (debugger->bridge && mScriptBridgeLookupSymbol(debugger->bridge, name, value)) {
		return true;
	}
#endif
	if (debugger->core->symbolTable && mDebuggerSymbolLookup(debugger->core->symbolTable, name, value, segment)) {
		return true;
	}
	if (debugger->core->lookupIdentifier(debugger->core, name, value, segment)) {
		return true;
	}
	if (debugger->platform && debugger->core->readRegister(debugger->core, name, value)) {
		return true;
	}
	return false;
}
