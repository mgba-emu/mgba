/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/geometry.h>

void mRectangleUnion(struct mRectangle* dst, const struct mRectangle* add) {
	int x0 = dst->x;
	int y0 = dst->y;
	int x1 = dst->x + dst->width;
	int y1 = dst->y + dst->height;

	if (add->x < x0) {
		x0 = add->x;
	}
	if (add->y < y0) {
		y0 = add->y;
	}
	if (add->x + add->width > x1) {
		x1 = add->x + add->width;
	}
	if (add->y + add->height > y1) {
		y1 = add->y + add->height;
	}

	dst->x = x0;
	dst->y = y0;
	dst->width = x1 - x0;
	dst->height = y1 - y0;
}

bool mRectangleIntersection(struct mRectangle* dst, const struct mRectangle* add) {
	int x[3];
	int y[3];

	if (dst == add) {
		return true;
	}

#define SORT(Z, M) \
	if (dst->Z < add->Z) { \
		Z[0] = dst->Z; \
		Z[1] = add->Z; \
	} else { \
		Z[0] = add->Z; \
		Z[1] = dst->Z; \
	} \
	if (dst->Z + dst->M < add->Z + add->M) { \
		/* dst is entirely before add */ \
		if (dst->Z + dst->M <= add->Z) { \
			return false; \
		} \
		if (dst->Z + dst->M < Z[1]) { \
			Z[2] = Z[1]; \
			Z[1] = dst->Z + dst->M; \
		} else { \
			Z[2] = dst->Z + dst->M; \
		} \
		if (add->Z + add->M < Z[2]) { \
			Z[2] = add->Z + add->M; \
		} \
	} else { \
		/* dst is after before add */ \
		if (dst->Z >= add->Z + add->M) { \
			return false; \
		} \
		if (add->Z + add->M < Z[1]) { \
			Z[2] = Z[1]; \
			Z[1] = add->Z + add->M; \
		} else { \
			Z[2] = add->Z + add->M; \
		} \
		if (dst->Z + dst->M < Z[2]) { \
			Z[2] = dst->Z + dst->M; \
		} \
	}

	SORT(x, width);
	SORT(y, height);

#undef SORT

	dst->x = x[1];
	dst->width = x[2] - x[1];
	dst->y = y[1];
	dst->height = y[2] - y[1];
	return true;
}

void mRectangleCenter(const struct mRectangle* ref, struct mRectangle* rect) {
	rect->x = ref->x + (ref->width - rect->width) / 2;
	rect->y = ref->y + (ref->height - rect->height) / 2;
}
