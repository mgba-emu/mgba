/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/matrix.h>

#define MTX_ONE 0x00001000

void DSGXMtxIdentity(struct DSGXMatrix* mtx) {
	memset(mtx, 0, sizeof(*mtx));
	mtx->m[0] = MTX_ONE;
	mtx->m[5] = MTX_ONE;
	mtx->m[10] = MTX_ONE;
	mtx->m[15] = MTX_ONE;
}
