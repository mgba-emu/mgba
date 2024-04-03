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
DEFINE_VECTOR(mDebuggerModuleList, struct mDebuggerModule*);

static void _mDebuggerInit(void* cpu, struct mCPUComponent* component);
static void _mDebuggerDeinit(struct mCPUComponent* component);

struct mDebuggerModule* mDebuggerCreateModule(enum mDebuggerType type, struct mCore* core) {
	if (!core->supportsDebuggerType(core, type)) {
		return NULL;
	}

	union DebugUnion {
		struct mDebuggerModule d;
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
		struct Address localHost = {
			.version = IPV4,
			.ipv4 = 0x7F000001
		};
		GDBStubListen(&debugger->gdb, 2345, &localHost, GDB_WATCHPOINT_STANDARD_LOGIC);
		break;
#endif
	case DEBUGGER_NONE:
	case DEBUGGER_ACCESS_LOGGER:
	case DEBUGGER_CUSTOM:
	case DEBUGGER_MAX:
		free(debugger);
		return NULL;
		break;
	}

	return &debugger->d;
}

void mDebuggerInit(struct mDebugger* debugger) {
	memset(debugger, 0, sizeof(*debugger));
	mDebuggerModuleListInit(&debugger->modules, 4);
	TableInit(&debugger->pointOwner, 0, NULL);
}

void mDebuggerDeinit(struct mDebugger* debugger) {
	mDebuggerModuleListDeinit(&debugger->modules);
	TableDeinit(&debugger->pointOwner);
}

void mDebuggerAttach(struct mDebugger* debugger, struct mCore* core) {
	debugger->d.id = DEBUGGER_ID;
	debugger->d.init = _mDebuggerInit;
	debugger->d.deinit = _mDebuggerDeinit;
	debugger->core = core;
	if (!debugger->core->symbolTable) {
		debugger->core->loadSymbols(debugger->core, NULL);
	}
	debugger->platform = core->debuggerPlatform(core);
	debugger->platform->p = debugger;
	core->attachDebugger(core, debugger);
}

void mDebuggerAttachModule(struct mDebugger* debugger, struct mDebuggerModule* module) {
	module->p = debugger;
	*mDebuggerModuleListAppend(&debugger->modules) = module;
	if (debugger->state > DEBUGGER_CREATED && debugger->state < DEBUGGER_SHUTDOWN) {
		if (module->init) {
			module->init(module);
		}
	}
}

void mDebuggerDetachModule(struct mDebugger* debugger, struct mDebuggerModule* module) {
	size_t i;
	for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
		if (module != *mDebuggerModuleListGetPointer(&debugger->modules, i)) {
			continue;
		}
		if (debugger->state > DEBUGGER_CREATED && debugger->state < DEBUGGER_SHUTDOWN) {
			if (module->deinit) {
				module->deinit(module);
			}
		}
		mDebuggerModuleListShift(&debugger->modules, i, 1);
		break;
	}
}

void mDebuggerRunTimeout(struct mDebugger* debugger, int32_t timeoutMs) {
	size_t i;
	size_t anyPaused = 0;

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
		for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
			struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
			if (module->needsCallback) {
				module->custom(module);
			}
		}
		break;
	case DEBUGGER_PAUSED:
		for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
			struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
			if (module->isPaused) {
				if (module->paused) {
					module->paused(module, timeoutMs);
				}
				if (module->isPaused) {
					++anyPaused;
				}
			} else if (module->needsCallback) {
				module->custom(module);
			}
		}
		if (debugger->state == DEBUGGER_PAUSED && !anyPaused) {
			debugger->state = DEBUGGER_RUNNING;
		}
		break;
	case DEBUGGER_CREATED:
		mLOG(DEBUGGER, ERROR, "Attempted to run debugger before initializtion");
		return;
	case DEBUGGER_SHUTDOWN:
		return;
	}
}

void mDebuggerRun(struct mDebugger* debugger) {
	mDebuggerRunTimeout(debugger, 50);
}

void mDebuggerRunFrame(struct mDebugger* debugger) {
	uint32_t frame = debugger->core->frameCounter(debugger->core);
	do {
		mDebuggerRun(debugger);
	} while (debugger->core->frameCounter(debugger->core) == frame);
}

void mDebuggerEnter(struct mDebugger* debugger, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	if (debugger->platform->entered) {
		debugger->platform->entered(debugger->platform, reason, info);
	}

	size_t i;
	for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
		struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
		if (info && info->target) {
			// This check needs to be in the loop to make sure we don't
			// accidentally enter a module that isn't registered.
			// This is an error by the caller, but it's good to check for.
			if (info->target != module) {
				continue;
			}
			// Make this the last loop so we don't hit this one twice
			i = mDebuggerModuleListSize(&debugger->modules) - 1;
		}
		module->isPaused = true;
		if (module->entered) {
			module->entered(module, reason, info);
		}
	}

#ifdef ENABLE_SCRIPTING
	if (debugger->bridge) {
		mScriptBridgeDebuggerEntered(debugger->bridge, reason, info);
	}
#endif

	mDebuggerUpdatePaused(debugger);
}

void mDebuggerInterrupt(struct mDebugger* debugger) {
	size_t i;
	for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
		struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
		if (module->interrupt) {
			module->interrupt(module);
		}
	}
}

void mDebuggerUpdatePaused(struct mDebugger* debugger) {
	if (debugger->state == DEBUGGER_SHUTDOWN) {
		return;
	}

	size_t anyPaused = 0;
	size_t anyCallback = 0;
	size_t i;
	for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
		struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
		if (module->isPaused) {
			++anyPaused;
		}
		if (module->needsCallback) {
			++anyCallback;
		}
	}
	if (anyPaused) {
		debugger->state = DEBUGGER_PAUSED;
	} else if (anyCallback) {
		debugger->state = DEBUGGER_CALLBACK;
	} else {
		debugger->state = DEBUGGER_RUNNING;
	}
}

void mDebuggerShutdown(struct mDebugger* debugger) {
	debugger->state = DEBUGGER_SHUTDOWN;
}

bool mDebuggerIsShutdown(const struct mDebugger* debugger) {
	return debugger->state == DEBUGGER_SHUTDOWN;
}

void mDebuggerUpdate(struct mDebugger* debugger) {
	size_t i;
	for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
		struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
		if (module->update) {
			module->update(module);
		}
	}
}

static void _mDebuggerInit(void* cpu, struct mCPUComponent* component) {
	struct mDebugger* debugger = (struct mDebugger*) component;
	debugger->state = DEBUGGER_RUNNING;
	debugger->platform->init(cpu, debugger->platform);

	size_t i;
	for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
		struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
		if (module->init) {
			module->init(module);
		}
	}
}

static void _mDebuggerDeinit(struct mCPUComponent* component) {
	struct mDebugger* debugger = (struct mDebugger*) component;
	debugger->state = DEBUGGER_SHUTDOWN;
	size_t i;
	for (i = 0; i < mDebuggerModuleListSize(&debugger->modules); ++i) {
		struct mDebuggerModule* module = *mDebuggerModuleListGetPointer(&debugger->modules, i);
		if (module->deinit) {
			module->deinit(module);
		}
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

void mDebuggerModuleSetNeedsCallback(struct mDebuggerModule* debugger) {
	debugger->needsCallback = true;
	mDebuggerUpdatePaused(debugger->p);
}
