/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_CLI_H
#define DS_CLI_H

#include "debugger/cli-debugger.h"

struct mCore;

struct DSCLIDebugger {
	struct CLIDebuggerSystem d;

	struct mCore* core;

	bool frameAdvance;
	bool inVblank;
};

struct DSCLIDebugger* DSCLIDebuggerCreate(struct mCore*);

#endif
