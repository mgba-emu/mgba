/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CLI_H
#define GBA_CLI_H

#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"

struct GBAThread;

struct GBACLIDebugger {
	struct CLIDebuggerSystem d;

	struct GBAThread* context;

	bool frameAdvance;
	bool inVblank;
};

struct GBACLIDebugger* GBACLIDebuggerCreate(struct GBAThread*);
#endif

#endif
