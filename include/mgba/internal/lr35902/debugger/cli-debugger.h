/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LR35902_CLI_DEBUGGER_H
#define LR35902_CLI_DEBUGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct CLIDebuggerSystem;
void LR35902CLIDebuggerCreate(struct CLIDebuggerSystem* debugger);

CXX_GUARD_END

#endif
