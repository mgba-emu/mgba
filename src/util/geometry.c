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

void mRectangleCenter(const struct mRectangle* ref, struct mRectangle* rect) {
	rect->x = ref->x + (ref->width - rect->width) / 2;
	rect->y = ref->y + (ref->height - rect->height) / 2;
}
