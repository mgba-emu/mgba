/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CLI_EL_BACKEND_H
#define CLI_EL_BACKEND_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/debugger/cli-debugger.h>

#include <histedit.h>

struct CLIDebuggerEditLineBackend {
	struct CLIDebuggerBackend d;

	EditLine* elstate;
	History* histate;
};

struct CLIDebuggerBackend* CLIDebuggerEditLineBackendCreate(void);

CXX_GUARD_END

#endif
