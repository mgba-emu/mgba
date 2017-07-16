/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/scripting.h>

#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

struct mScriptBridge {
	struct Table engines;
	struct mDebugger* debugger;
};

struct mScriptInfo {
	const char* name;
	struct VFile* vf;
	bool success;
};

struct mScriptSymbol {
	const char* name;
	int32_t* out;
	bool success;
};

static void _seDeinit(void* value) {
	struct mScriptEngine* se = value;
	se->deinit(se);
}

static void _seTryLoad(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngine* se = value;
	struct mScriptInfo* si = user;
	if (!si->success && se->isScript(se, si->name, si->vf)) {
		si->success = se->loadScript(se, si->name, si->vf);
	}
}

static void _seLookupSymbol(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngine* se = value;
	struct mScriptSymbol* si = user;
	if (!si->success) {
		si->success = se->lookupSymbol(se, si->name, si->out);
	}
}

static void _seRun(const char* key, void* value, void* user) {
	UNUSED(key);
	UNUSED(user);
	struct mScriptEngine* se = value;
	se->run(se);
}

#ifdef USE_DEBUGGERS
struct mScriptDebuggerEntry {
	enum mDebuggerEntryReason reason;
	struct mDebuggerEntryInfo* info;
};

static void _seDebuggerEnter(const char* key, void* value, void* user) {
	UNUSED(key);
	struct mScriptEngine* se = value;
	struct mScriptDebuggerEntry* entry = user;
	se->debuggerEntered(se, entry->reason, entry->info);
}
#endif

struct mScriptBridge* mScriptBridgeCreate(void) {
	struct mScriptBridge* sb = malloc(sizeof(*sb));
	HashTableInit(&sb->engines, 0, _seDeinit);
	sb->debugger = NULL;
	return sb;
}

void mScriptBridgeDestroy(struct mScriptBridge* sb) {
	HashTableDeinit(&sb->engines);
	free(sb);
}

void mScriptBridgeInstallEngine(struct mScriptBridge* sb, struct mScriptEngine* se) {
	if (!se->init(se, sb)) {
		return;
	}
	const char* name = se->name(se);
	HashTableInsert(&sb->engines, name, se);
}

#ifdef USE_DEBUGGERS
void mScriptBridgeSetDebugger(struct mScriptBridge* sb, struct mDebugger* debugger) {
	sb->debugger = debugger;
	debugger->bridge = sb;
}

struct mDebugger* mScriptBridgeGetDebugger(struct mScriptBridge* sb) {
	return sb->debugger;
}

void mScriptBridgeDebuggerEntered(struct mScriptBridge* sb, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct mScriptDebuggerEntry entry = {
		.reason = reason,
		.info = info
	};
	HashTableEnumerate(&sb->engines, _seDebuggerEnter, &entry);
}
#endif

void mScriptBridgeRun(struct mScriptBridge* sb) {
	HashTableEnumerate(&sb->engines, _seRun, NULL);
}

bool mScriptBridgeLoadScript(struct mScriptBridge* sb, const char* name) {
	struct VFile* vf = VFileOpen(name, O_RDONLY);
	if (!vf) {
		return false;
	}
	struct mScriptInfo info = {
		.name = name,
		.vf = vf,
		.success = false
	};
	HashTableEnumerate(&sb->engines, _seTryLoad, &info);
	vf->close(vf);
	return info.success;
}

bool mScriptBridgeLookupSymbol(struct mScriptBridge* sb, const char* name, int32_t* out) {
	struct mScriptSymbol info = {
		.name = name,
		.out = out,
		.success = false
	};
	HashTableEnumerate(&sb->engines, _seLookupSymbol, &info);
	return info.success;
}
