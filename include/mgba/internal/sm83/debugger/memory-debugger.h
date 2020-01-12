/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SM83_MEMORY_DEBUGGER_H
#define SM83_MEMORY_DEBUGGER_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct SM83Debugger;

void SM83DebuggerInstallMemoryShim(struct SM83Debugger* debugger);
void SM83DebuggerRemoveMemoryShim(struct SM83Debugger* debugger);

CXX_GUARD_END

#endif
