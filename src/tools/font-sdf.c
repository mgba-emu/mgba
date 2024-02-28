/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/image.h>
#include <mgba-util/gui/font-metrics.h>
#include <mgba-util/math.h>

void createSdf(const struct mImage* src, struct mImage* dst, int x, int y, int w, int h) {
	int i, j, ii, jj, z;

	static const int kernel[] = {
		11, 9, 8, 9, 11,
		 9, 6, 4, 6,  9,
		 8, 4, 0, 4,  8,
		 9, 6, 4, 6,  9,
		11, 9, 8, 9, 11,
	};
	const int* kc = &kernel[12];

	for (j = y; j < y + h; ++j) {
		for (i = x; i < x + w; ++i) {
			if (mImageGetPixel(src, i, j) & 0xFFFFFF) {
				mImageSetPixelRaw(dst, i, j, 0xFF);
			} else {
				mImageSetPixelRaw(dst, i, j, 1);		
			}
		}
	}

	for (z = 0; z < 16; ++z) {
		int left = w * h;
		for (j = y; j < y + h; ++j) {
			for (i = x; i < x + w; ++i) {
				int raw = mImageGetPixelRaw(dst, i, j) - 0x80;
				int neighbors = raw;
				for (jj = -2; jj < 3; ++jj) {
					for (ii = -2; ii < 3; ++ii) {
						if (!ii && !jj) {
							continue;
						}
						if (i + ii - x < 0 || j + jj - y < 0) {
							continue;
						}
						if (i + ii - x >= w || j + jj - y >= h) {
							continue;
						}
						int neighbor = mImageGetPixelRaw(dst, i + ii, j + jj) - 0x80;
						if (raw > 0) {
							if (neighbor < 0) {
								if ((!ii && (jj == -1 || jj == 1)) || (!jj && (ii == -1 || ii == 1))) {
									neighbors = 1;
									jj = 5;
									break;
								}
								continue;
							}
							if (neighbor == 0x7F) {
								continue;
							}
							if (neighbor + kc[ii + jj * 5] < neighbors) {
								neighbors = neighbor + kc[ii + jj * 5];
							}
						} else if (raw < 0) {
							if (neighbor > 0) {
								if ((!ii && (jj == -1 || jj == 1)) || (!jj && (ii == -1 || ii == 1))) {
									neighbors = -4;
									jj = 5;
									break;
								}
								continue;
							}
							if (neighbor == -0x7F) {
								continue;
							}
							if (neighbor - kc[ii + jj * 5] > neighbors) {
								neighbors = neighbor - kc[ii + jj * 5];
							}
						}
					}
				}
				if (neighbors == raw) {
					--left;
				} else {
					mImageSetPixelRaw(dst, i, j, neighbors + 0x80);
				}
				if (!left) {
					break;
				}
			}
			if (!left) {
				break;
			}
		}
		if (!left) {
			break;
		}
	}
}

int main(int argc, char* argv[]) {
	struct mImage* source;
	struct mImage* output;

	if (argc != 3) {
		return 1;
	}
	source = mImageLoad(argv[1]);
	if (!source) {
		return 2;
	}
	output = mImageCreate(source->width, source->height, mCOLOR_L8);
	if (!output) {
		return 3;
	}

	int i;
	for (i = 0; i < 128; ++i) {
		createSdf(source, output, (i & 0xF) << 6, (i & ~0xF) << 2, 64, 64);
	}
	for (i = 0; i < GUI_ICON_MAX; ++i) {
		struct GUIIconMetric metric = defaultIconMetrics[i];
		metric.width += metric.x & 0xF;
		metric.height += metric.y & 0xF;
		metric.x &= ~0xF;
		metric.y &= ~0xF;
		createSdf(source, output, metric.x * 4, metric.y * 4 + 512, toPow2(metric.width) * 4, toPow2(metric.height) * 4);
	}
	if (!mImageSave(output, argv[2], NULL)) {
		return 4;
	}
	mImageDestroy(source);
	mImageDestroy(output);
	return 0;
}
