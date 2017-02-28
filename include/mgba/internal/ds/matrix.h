/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_MATRIX_H
#define DS_MATRIX_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct DSGXMatrix {
	int32_t m[16]; // 20.12
};

void DSGXMtxIdentity(struct DSGXMatrix*);

CXX_GUARD_END

#endif
