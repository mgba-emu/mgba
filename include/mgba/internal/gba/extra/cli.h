/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CLI_H
#define GBA_CLI_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/debugger/cli-debugger.h>

struct mCore;

struct GBACLIDebugger {
	struct CLIDebuggerSystem d;

	struct mCore* core;

	bool frameAdvance;
	bool inVblank;
};

struct GBACLIDebugger* GBACLIDebuggerCreate(struct mCore*);

CXX_GUARD_END

#endif
