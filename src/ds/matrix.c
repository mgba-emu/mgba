/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/matrix.h>

static int32_t _dot(const int32_t* col, const int32_t* row) {
	int64_t a;
	int64_t b;
	int64_t sum;
	a = col[0];
	b = row[0];
	sum = a * b;
	a = col[4];
	b = row[1];
	sum += a * b;
	a = col[8];
	b = row[2];
	sum += a * b;
	a = col[12];
	b = row[3];
	sum += a * b;
	return sum >> 12LL;
}

void DSGXMtxIdentity(struct DSGXMatrix* mtx) {
	memset(mtx, 0, sizeof(*mtx));
	mtx->m[0] = MTX_ONE;
	mtx->m[5] = MTX_ONE;
	mtx->m[10] = MTX_ONE;
	mtx->m[15] = MTX_ONE;
}

void DSGXMtxMultiply(struct DSGXMatrix* out, const struct DSGXMatrix* a, const struct DSGXMatrix* b) {
	struct DSGXMatrix o;
	// XXX: This is transposed because DS matrices are transposed
	o.m[0] = _dot(&a->m[0], &b->m[0]);
	o.m[1] = _dot(&a->m[1], &b->m[0]);
	o.m[2] = _dot(&a->m[2], &b->m[0]);
	o.m[3] = _dot(&a->m[3], &b->m[0]);
	o.m[4] = _dot(&a->m[0], &b->m[4]);
	o.m[5] = _dot(&a->m[1], &b->m[4]);
	o.m[6] = _dot(&a->m[2], &b->m[4]);
	o.m[7] = _dot(&a->m[3], &b->m[4]);
	o.m[8] = _dot(&a->m[0], &b->m[8]);
	o.m[9] = _dot(&a->m[1], &b->m[8]);
	o.m[10] = _dot(&a->m[2], &b->m[8]);
	o.m[11] = _dot(&a->m[3], &b->m[8]);
	o.m[12] = _dot(&a->m[0], &b->m[12]);
	o.m[13] = _dot(&a->m[1], &b->m[12]);
	o.m[14] = _dot(&a->m[2], &b->m[12]);
	o.m[15] = _dot(&a->m[3], &b->m[12]);
	*out = o;
}

void DSGXMtxScale(struct DSGXMatrix* mtx, const int32_t* m) {
	struct DSGXMatrix s = {
		.m = {
			m[0], 0, 0, 0,
			0, m[1], 0, 0,
			0, 0, m[2], 0,
			0, 0, 0, MTX_ONE
		}
	};
	DSGXMtxMultiply(mtx, &s, mtx);
}

void DSGXMtxTranslate(struct DSGXMatrix* mtx, const int32_t* m) {
	struct DSGXMatrix t = {
		.m = {
			MTX_ONE, 0, 0, 0,
			0, MTX_ONE, 0, 0,
			0, 0, MTX_ONE, 0,
			m[0], m[1], m[2], MTX_ONE
		}
	};
	DSGXMtxMultiply(mtx, &t, mtx);
}
