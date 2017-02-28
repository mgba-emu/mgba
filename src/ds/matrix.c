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

void DSGXMtxMultiply(struct DSGXMatrix* mtx, const struct DSGXMatrix* m) {
	struct DSGXMatrix out;
	out.m[0] = _dot(&mtx->m[0], &m->m[0]);
	out.m[1] = _dot(&mtx->m[1], &m->m[0]);
	out.m[2] = _dot(&mtx->m[2], &m->m[0]);
	out.m[3] = _dot(&mtx->m[3], &m->m[0]);
	out.m[4] = _dot(&mtx->m[0], &m->m[4]);
	out.m[5] = _dot(&mtx->m[1], &m->m[4]);
	out.m[6] = _dot(&mtx->m[2], &m->m[4]);
	out.m[7] = _dot(&mtx->m[3], &m->m[4]);
	out.m[8] = _dot(&mtx->m[0], &m->m[8]);
	out.m[9] = _dot(&mtx->m[1], &m->m[8]);
	out.m[10] = _dot(&mtx->m[2], &m->m[8]);
	out.m[11] = _dot(&mtx->m[3], &m->m[8]);
	out.m[12] = _dot(&mtx->m[0], &m->m[12]);
	out.m[13] = _dot(&mtx->m[1], &m->m[12]);
	out.m[14] = _dot(&mtx->m[2], &m->m[12]);
	out.m[15] = _dot(&mtx->m[3], &m->m[12]);
	*mtx = out;
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
	DSGXMtxMultiply(mtx, &s);
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
	DSGXMtxMultiply(mtx, &t);
}
