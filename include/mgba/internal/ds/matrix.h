/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_MATRIX_H
#define DS_MATRIX_H

#include <mgba-util/common.h>

CXX_GUARD_START

#define MTX_ONE 0x00001000

struct DSGXMatrix {
	int32_t m[16]; // 20.12
};

void DSGXMtxIdentity(struct DSGXMatrix*);
void DSGXMtxMultiply(struct DSGXMatrix*, const struct DSGXMatrix*);
void DSGXMtxScale(struct DSGXMatrix*, const int32_t* m);
void DSGXMtxTranslate(struct DSGXMatrix*, const int32_t* m);

CXX_GUARD_END

#endif
