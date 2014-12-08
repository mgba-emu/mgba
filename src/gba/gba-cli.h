/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CLI_H
#define GBA_CLI_H

#include "debugger/cli-debugger.h"

struct GBAThread;

struct GBACLIDebugger {
#ifdef USE_CLI_DEBUGGER
	struct CLIDebuggerSystem d;

	struct GBAThread* context;
#endif
};

struct GBACLIDebugger* GBACLIDebuggerCreate(struct GBAThread*);

#endif
