/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_TYPES_INTERNAL_H
#define M_SCRIPT_TYPES_INTERNAL_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct Table;
void mScriptContextGetInputTypes(struct Table*);

void mScriptTypeAdd(struct Table*, const struct mScriptType* type);

CXX_GUARD_END

#endif
