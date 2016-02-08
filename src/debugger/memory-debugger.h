/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MEMORY_DEBUGGER_H
#define MEMORY_DEBUGGER_H

#include "util/common.h"

#include "arm.h"

struct Debugger;

void DebuggerInstallMemoryShim(struct Debugger* debugger);
void DebuggerRemoveMemoryShim(struct Debugger* debugger);

#endif
