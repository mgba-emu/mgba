/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba-util/image.h>
#ifdef USE_PNG
#include <mgba-util/image/png-io.h>
#endif
#include <mgba-util/vfs.h>

M_TEST_DEFINE(zeroDim) {
	assert_null(mImageCreate(0, 0, mCOLOR_ABGR8));
	assert_null(mImageCreate(1, 0, mCOLOR_ABGR8));
	assert_null(mImageCreate(0, 1, mCOLOR_ABGR8));
	struct mImage* image = mImageCreate(1, 1, mCOLOR_ABGR8);
	assert_non_null(image);
	mImageDestroy(image);
}

M_TEST_DEFINE(pitchRead) {
	static uint8_t buffer[12] = {
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB
	};

	struct mImage image = {
		.data = buffer,
		.height = 1
	};
	int i;

	image.depth = 1;
	image.width = 12;
	image.format = mCOLOR_L8;

	for (i = 0; i < 12; ++i) {
		assert_int_equal(mImageGetPixelRaw(&image, i, 0), i);
	}

	image.depth = 2;
	image.width = 6;
	image.format = mCOLOR_RGB5;

	for (i = 0; i < 6; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, i, 0), (i * 2) << 8 | (i * 2 + 1));
#else
		assert_int_equal(mImageGetPixelRaw(&image, i, 0), (i * 2 + 1) << 8 | (i * 2));
#endif
	}

	image.depth = 3;
	image.width = 4;
	image.format = mCOLOR_RGB8;

	for (i = 0; i < 4; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, i, 0), (i * 3) << 16 | (i * 3 + 1) << 8 | (i * 3 + 2));
#else
		assert_int_equal(mImageGetPixelRaw(&image, i, 0), (i * 3 + 2) << 16 | (i * 3 + 1) << 8 | (i * 3));
#endif
	}

	image.depth = 4;
	image.width = 3;
	image.format = mCOLOR_ARGB8;

	for (i = 0; i < 3; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, i, 0), (i * 4) << 24 | (i * 4 + 1) << 16 | (i * 4 + 2) << 8 | (i * 4 + 3));
#else
		assert_int_equal(mImageGetPixelRaw(&image, i, 0), (i * 4 + 3) << 24 | (i * 4 + 2) << 16 | (i * 4 + 1) << 8 | (i * 4));
#endif
	}
}

M_TEST_DEFINE(strideRead) {
	static uint8_t buffer[12] = {
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB
	};

	struct mImage image = {
		.data = buffer,
		.width = 1
	};
	int i;

	image.depth = 1;
	image.stride = 1;
	image.height = 12;
	image.format = mCOLOR_L8;

	for (i = 0; i < 12; ++i) {
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), i);
	}

	image.depth = 1;
	image.stride = 2;
	image.height = 6;
	image.format = mCOLOR_L8;

	for (i = 0; i < 6; ++i) {
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), i * 2);
	}

	image.depth = 2;
	image.stride = 1;
	image.height = 6;
	image.format = mCOLOR_RGB5;

	for (i = 0; i < 6; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 2) << 8 | (i * 2 + 1));
#else
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 2 + 1) << 8 | (i * 2));
#endif
	}

	image.depth = 2;
	image.stride = 2;
	image.height = 3;
	image.format = mCOLOR_RGB5;

	for (i = 0; i < 3; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 4) << 8 | (i * 4 + 1));
#else
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 4 + 1) << 8 | (i * 4));
#endif
	}

	image.depth = 3;
	image.stride = 1;
	image.height = 4;
	image.format = mCOLOR_RGB8;

	for (i = 0; i < 4; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 3) << 16 | (i * 3 + 1) << 8 | (i * 3 + 2));
#else
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 3 + 2) << 16 | (i * 3 + 1) << 8 | (i * 3));
#endif
	}

	image.depth = 3;
	image.stride = 2;
	image.height = 2;
	image.format = mCOLOR_RGB8;

	for (i = 0; i < 2; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 6) << 16 | (i * 6 + 1) << 8 | (i * 6 + 2));
#else
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 6 + 2) << 16 | (i * 6 + 1) << 8 | (i * 6));
#endif
	}

	image.depth = 4;
	image.stride = 1;
	image.height = 3;
	image.format = mCOLOR_ARGB8;

	for (i = 0; i < 3; ++i) {
#ifdef __BIG_ENDIAN__
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 4) << 24 | (i * 4 + 1) << 16 | (i * 4 + 2) << 8 | (i * 4 + 3));
#else
		assert_int_equal(mImageGetPixelRaw(&image, 0, i), (i * 4 + 3) << 24 | (i * 4 + 2) << 16 | (i * 4 + 1) << 8 | (i * 4));
#endif
	}
}

M_TEST_DEFINE(oobRead) {
	static uint8_t buffer[8] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

	struct mImage image = {
		.data = buffer,
		.width = 1,
		.height = 1,
		.stride = 1
	};

	image.depth = 1;
	image.format = mCOLOR_L8;

	assert_int_equal(mImageGetPixelRaw(&image, 0, 0), 0xFF);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 0), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 0, 1), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 1), 0);

	image.depth = 2;
	image.format = mCOLOR_RGB5;

	assert_int_equal(mImageGetPixelRaw(&image, 0, 0), 0xFFFF);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 0), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 0, 1), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 1), 0);

	image.depth = 3;
	image.format = mCOLOR_RGB8;

	assert_int_equal(mImageGetPixelRaw(&image, 0, 0), 0xFFFFFF);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 0), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 0, 1), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 1), 0);

	image.depth = 4;
	image.format = mCOLOR_ARGB8;

	assert_int_equal(mImageGetPixelRaw(&image, 0, 0), 0xFFFFFFFF);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 0), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 0, 1), 0);
	assert_int_equal(mImageGetPixelRaw(&image, 1, 1), 0);
}

M_TEST_DEFINE(pitchWrite) {
	static const uint8_t baseline[12] = {
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB
	};

	uint8_t buffer[12];

	struct mImage image = {
		.data = buffer,
		.height = 1
	};
	int i;

	image.depth = 1;
	image.width = 12;
	image.format = mCOLOR_L8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 12; ++i) {
		mImageSetPixelRaw(&image, i, 0, i);
	}
	assert_memory_equal(baseline, buffer, sizeof(baseline));

	image.depth = 2;
	image.width = 6;
	image.format = mCOLOR_RGB5;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 6; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, i, 0, (i * 2) << 8 | (i * 2 + 1));
#else
		mImageSetPixelRaw(&image, i, 0, (i * 2 + 1) << 8 | (i * 2));
#endif
	}
	assert_memory_equal(baseline, buffer, sizeof(baseline));

	image.depth = 3;
	image.width = 4;
	image.format = mCOLOR_RGB8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 4; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, i, 0, (i * 3) << 16 | (i * 3 + 1) << 8 | (i * 3 + 2));
#else
		mImageSetPixelRaw(&image, i, 0, (i * 3 + 2) << 16 | (i * 3 + 1) << 8 | (i * 3));
#endif
	}
	assert_memory_equal(baseline, buffer, sizeof(baseline));

	image.depth = 4;
	image.width = 3;
	image.format = mCOLOR_ARGB8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 3; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, i, 0, (i * 4) << 24 | (i * 4 + 1) << 16 | (i * 4 + 2) << 8 | (i * 4 + 3));
#else
		mImageSetPixelRaw(&image, i, 0, (i * 4 + 3) << 24 | (i * 4 + 2) << 16 | (i * 4 + 1) << 8 | (i * 4));
#endif
	}
	assert_memory_equal(baseline, buffer, sizeof(baseline));
}

M_TEST_DEFINE(strideWrite) {
	static const uint8_t baseline[12] = {
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB
	};
	static const uint8_t baseline2x1[12] = {
		0x0, 0x0, 0x2, 0x0, 0x4, 0x0, 0x6, 0x0, 0x8, 0x0, 0xA, 0x0
	};
	static const uint8_t baseline2x2[12] = {
		0x0, 0x1, 0x0, 0x0, 0x4, 0x5, 0x0, 0x0, 0x8, 0x9, 0x0, 0x0
	};
	static const uint8_t baseline3x2[12] = {
		0x0, 0x1, 0x2, 0x0, 0x0, 0x0, 0x6, 0x7, 0x8, 0x0, 0x0, 0x0
	};

	uint8_t buffer[12];

	struct mImage image = {
		.data = buffer,
		.width = 1
	};
	int i;

	image.depth = 1;
	image.stride = 1;
	image.height = 12;
	image.format = mCOLOR_L8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 12; ++i) {
		mImageSetPixelRaw(&image, 0, i, i);
	}
	assert_memory_equal(baseline, buffer, sizeof(buffer));

	image.depth = 1;
	image.stride = 2;
	image.height = 6;
	image.format = mCOLOR_L8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 6; ++i) {
		mImageSetPixelRaw(&image, 0, i, i * 2);
	}
	assert_memory_equal(baseline2x1, buffer, sizeof(buffer));

	image.depth = 2;
	image.stride = 1;
	image.height = 6;
	image.format = mCOLOR_RGB5;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 6; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, 0, i, (i * 2) << 8 | (i * 2 + 1));
#else
		mImageSetPixelRaw(&image, 0, i, (i * 2 + 1) << 8 | (i * 2));
#endif
	}
	assert_memory_equal(baseline, buffer, sizeof(buffer));

	image.depth = 2;
	image.stride = 2;
	image.height = 3;
	image.format = mCOLOR_RGB5;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 3; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, 0, i, (i * 4) << 8 | (i * 4 + 1));
#else
		mImageSetPixelRaw(&image, 0, i, (i * 4 + 1) << 8 | (i * 4));
#endif
	}
	assert_memory_equal(baseline2x2, buffer, sizeof(buffer));

	image.depth = 3;
	image.stride = 1;
	image.height = 4;
	image.format = mCOLOR_RGB8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 4; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, 0, i, (i * 3) << 16 | (i * 3 + 1) << 8 | (i * 3 + 2));
#else
		mImageSetPixelRaw(&image, 0, i, (i * 3 + 2) << 16 | (i * 3 + 1) << 8 | (i * 3));
#endif
	}
	assert_memory_equal(baseline, buffer, sizeof(buffer));

	image.depth = 3;
	image.stride = 2;
	image.height = 2;
	image.format = mCOLOR_RGB8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 2; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, 0, i, (i * 6) << 16 | (i * 6 + 1) << 8 | (i * 6 + 2));
#else
		mImageSetPixelRaw(&image, 0, i, (i * 6 + 2) << 16 | (i * 6 + 1) << 8 | (i * 6));
#endif
	}
	assert_memory_equal(baseline3x2, buffer, sizeof(buffer));

	image.depth = 4;
	image.stride = 1;
	image.height = 3;
	image.format = mCOLOR_ARGB8;

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; i < 3; ++i) {
#ifdef __BIG_ENDIAN__
		mImageSetPixelRaw(&image, 0, i, (i * 4) << 24 | (i * 4 + 1) << 16 | (i * 4 + 2) << 8 | (i * 4 + 3));
#else
		mImageSetPixelRaw(&image, 0, i, (i * 4 + 3) << 24 | (i * 4 + 2) << 16 | (i * 4 + 1) << 8 | (i * 4));
#endif
	}
	assert_memory_equal(baseline, buffer, sizeof(buffer));
}

M_TEST_DEFINE(oobWrite) {
	static uint8_t buffer[8];

	struct mImage image = {
		.data = buffer,
		.width = 1,
		.height = 1,
		.stride = 1
	};

	image.depth = 1;
	image.format = mCOLOR_L8;

	memset(buffer, 0, sizeof(buffer));
	mImageSetPixelRaw(&image, 0, 0, 0xFF);
	mImageSetPixelRaw(&image, 1, 0, 0);
	mImageSetPixelRaw(&image, 0, 1, 0);
	mImageSetPixelRaw(&image, 1, 1, 0);
	assert_memory_equal(buffer, (&(uint8_t[8]) { 0xFF }), sizeof(buffer));

	image.depth = 2;
	image.format = mCOLOR_RGB5;

	memset(buffer, 0, sizeof(buffer));
	mImageSetPixelRaw(&image, 0, 0, 0xFFFF);
	mImageSetPixelRaw(&image, 1, 0, 0);
	mImageSetPixelRaw(&image, 0, 1, 0);
	mImageSetPixelRaw(&image, 1, 1, 0);
	assert_memory_equal(buffer, (&(uint8_t[8]) { 0xFF, 0xFF }), sizeof(buffer));

	image.depth = 3;
	image.format = mCOLOR_RGB8;

	memset(buffer, 0, sizeof(buffer));
	mImageSetPixelRaw(&image, 0, 0, 0xFFFFFF);
	mImageSetPixelRaw(&image, 1, 0, 0);
	mImageSetPixelRaw(&image, 0, 1, 0);
	mImageSetPixelRaw(&image, 1, 1, 0);
	assert_memory_equal(buffer, (&(uint8_t[8]) { 0xFF, 0xFF, 0xFF }), sizeof(buffer));

	image.depth = 4;
	image.format = mCOLOR_ARGB8;

	memset(buffer, 0, sizeof(buffer));
	mImageSetPixelRaw(&image, 0, 0, 0xFFFFFFFF);
	mImageSetPixelRaw(&image, 1, 0, 0);
	mImageSetPixelRaw(&image, 0, 1, 0);
	mImageSetPixelRaw(&image, 1, 1, 0);
	assert_memory_equal(buffer, (&(uint8_t[8]) { 0xFF, 0xFF, 0xFF, 0xFF }), sizeof(buffer));
}

M_TEST_DEFINE(paletteAccess) {
	struct mImage* image = mImageCreate(1, 1, mCOLOR_PAL8);
	mImageSetPaletteSize(image, 1);

	mImageSetPaletteEntry(image, 0, 0xFF00FF00);
	mImageSetPixelRaw(image, 0, 0, 0);
	assert_int_equal(mImageGetPixelRaw(image, 0, 0), 0);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0xFF00FF00);

	mImageSetPaletteEntry(image, 0, 0x01234567);
	assert_int_equal(mImageGetPixelRaw(image, 0, 0), 0);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0x01234567);

	mImageDestroy(image);
}

#ifdef USE_PNG
M_TEST_DEFINE(loadPng24) {
	const uint8_t data[] = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
		0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
		0x08, 0x02, 0x00, 0x00, 0x00, 0xfd, 0xd4, 0x9a, 0x73, 0x00, 0x00, 0x00,
		0x12, 0x49, 0x44, 0x41, 0x54, 0x08, 0x99, 0x63, 0xf8, 0xff, 0xff, 0x3f,
		0x03, 0x03, 0x03, 0x03, 0x84, 0x00, 0x00, 0x2a, 0xe3, 0x04, 0xfc, 0xe8,
		0x51, 0xc0, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
		0x42, 0x60, 0x82
	};

	struct VFile* vf = VFileFromConstMemory(data, sizeof(data));
	struct mImage* image = mImageLoadVF(vf);
	vf->close(vf);

	assert_non_null(image);
	assert_int_equal(image->width, 2);
	assert_int_equal(image->height, 2);
	assert_int_equal(image->format, mCOLOR_XBGR8);

	assert_int_equal(mImageGetPixel(image, 0, 0) & 0xFFFFFF, 0xFFFFFF);
	assert_int_equal(mImageGetPixel(image, 1, 0) & 0xFFFFFF, 0x000000);
	assert_int_equal(mImageGetPixel(image, 0, 1) & 0xFFFFFF, 0xFF0000);
	assert_int_equal(mImageGetPixel(image, 1, 1) & 0xFFFFFF, 0x0000FF);

	mImageDestroy(image);
}

M_TEST_DEFINE(loadPng32) {
	const uint8_t data[] = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
		0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
		0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d, 0x24, 0x00, 0x00, 0x00,
		0x1a, 0x49, 0x44, 0x41, 0x54, 0x08, 0x99, 0x05, 0xc1, 0x31, 0x01, 0x00,
		0x00, 0x08, 0xc0, 0x20, 0x6c, 0x66, 0x25, 0xfb, 0x1f, 0x13, 0xa6, 0x0a,
		0xa7, 0x5a, 0x78, 0x58, 0x7b, 0x07, 0xac, 0xe9, 0x00, 0x3d, 0x95, 0x00,
		0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
	};

	struct VFile* vf = VFileFromConstMemory(data, sizeof(data));
	struct mImage* image = mImageLoadVF(vf);
	vf->close(vf);

	assert_non_null(image);
	assert_int_equal(image->width, 2);
	assert_int_equal(image->height, 2);
	assert_int_equal(image->format, mCOLOR_ABGR8);

	assert_int_equal(mImageGetPixel(image, 0, 0) >> 24, 0xFF);
	assert_int_equal(mImageGetPixel(image, 1, 0) >> 24, 0x70);
	assert_int_equal(mImageGetPixel(image, 0, 1) >> 24, 0x40);
	assert_int_equal(mImageGetPixel(image, 1, 1) >> 24, 0x00);

	mImageDestroy(image);
}

M_TEST_DEFINE(loadPngPalette) {
	const uint8_t data[] = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
		0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
		0x08, 0x03, 0x00, 0x00, 0x00, 0x45, 0x68, 0xfd, 0x16, 0x00, 0x00, 0x00,
		0x0c, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
		0xff, 0x00, 0x00, 0x00, 0xff, 0x9b, 0xc0, 0x13, 0xdc, 0x00, 0x00, 0x00,
		0x04, 0x74, 0x52, 0x4e, 0x53, 0x00, 0xc0, 0x80, 0x40, 0x6f, 0x63, 0x29,
		0x01, 0x00, 0x00, 0x00, 0x0e, 0x49, 0x44, 0x41, 0x54, 0x08, 0x99, 0x63,
		0x60, 0x60, 0x64, 0x60, 0x62, 0x06, 0x00, 0x00, 0x11, 0x00, 0x07, 0x69,
		0xe2, 0x2a, 0x44, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
		0x42, 0x60, 0x82
	};

	struct VFile* vf = VFileFromConstMemory(data, sizeof(data));
	struct mImage* image = mImageLoadVF(vf);
	vf->close(vf);

	assert_non_null(image);
	assert_int_equal(image->width, 2);
	assert_int_equal(image->height, 2);
	assert_int_equal(image->format, mCOLOR_PAL8);

	assert_int_equal(mImageGetPixel(image, 0, 0), 0x00000000);
	assert_int_equal(mImageGetPixel(image, 1, 0), 0xC0FF0000);
	assert_int_equal(mImageGetPixel(image, 0, 1), 0x8000FF00);
	assert_int_equal(mImageGetPixel(image, 1, 1), 0x400000FF);

	mImageDestroy(image);
}

M_TEST_DEFINE(savePngNative) {
	struct mImage* image = mImageCreate(1, 1, mCOLOR_ABGR8);
	mImageSetPixel(image, 0, 0, 0x01234567);

	struct VFile* vf = VFileMemChunk(NULL, 0);
	assert_true(mImageSaveVF(image, vf, "png"));
	mImageDestroy(image);

	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);
	assert_true(isPNG(vf));
	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);

	image = mImageLoadVF(vf);
	vf->close(vf);
	assert_non_null(image);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0x01234567);
	mImageDestroy(image);
}

M_TEST_DEFINE(savePngNonNative) {
	struct mImage* image = mImageCreate(1, 1, mCOLOR_ARGB8);
	mImageSetPixel(image, 0, 0, 0x01234567);

	struct VFile* vf = VFileMemChunk(NULL, 0);
	assert_true(mImageSaveVF(image, vf, "png"));
	mImageDestroy(image);

	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);
	assert_true(isPNG(vf));
	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);

	image = mImageLoadVF(vf);
	vf->close(vf);
	assert_non_null(image);
	assert_int_equal(image->format, mCOLOR_ABGR8);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0x01234567);
	mImageDestroy(image);
}

M_TEST_DEFINE(savePngRoundTrip) {
	const enum mColorFormat formats[] = {
		mCOLOR_XBGR8, mCOLOR_XRGB8,
		mCOLOR_BGRX8, mCOLOR_RGBX8,
		mCOLOR_ABGR8, mCOLOR_ARGB8,
		mCOLOR_BGRA8, mCOLOR_RGBA8,
		mCOLOR_RGB5, mCOLOR_BGR5,
		mCOLOR_ARGB5, mCOLOR_ABGR5,
		mCOLOR_RGBA5, mCOLOR_BGRA5,
		mCOLOR_RGB565, mCOLOR_BGR565,
		mCOLOR_RGB8, mCOLOR_BGR8,
		0
	};

	int i;
	for (i = 0; formats[i]; ++i) {
		struct mImage* image = mImageCreate(2, 2, formats[i]);
		mImageSetPixel(image, 0, 0, 0xFF181008);
		mImageSetPixel(image, 1, 0, 0xFF100818);
		mImageSetPixel(image, 0, 1, 0xFF081810);
		mImageSetPixel(image, 1, 1, 0xFF181008);
		assert_int_equal(mImageGetPixel(image, 0, 0), 0xFF181008);
		assert_int_equal(mImageGetPixel(image, 1, 0), 0xFF100818);
		assert_int_equal(mImageGetPixel(image, 0, 1), 0xFF081810);
		assert_int_equal(mImageGetPixel(image, 1, 1), 0xFF181008);

		struct VFile* vf = VFileMemChunk(NULL, 0);
		assert_true(mImageSaveVF(image, vf, "png"));
		mImageDestroy(image);

		assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);
		assert_true(isPNG(vf));
		assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);

		image = mImageLoadVF(vf);
		vf->close(vf);
		assert_non_null(image);
		assert_int_equal(mImageGetPixel(image, 0, 0), 0xFF181008);
		assert_int_equal(mImageGetPixel(image, 1, 0), 0xFF100818);
		assert_int_equal(mImageGetPixel(image, 0, 1), 0xFF081810);
		assert_int_equal(mImageGetPixel(image, 1, 1), 0xFF181008);
		mImageDestroy(image);
	}
}

M_TEST_DEFINE(savePngL8) {
	struct mImage* image = mImageCreate(2, 2, mCOLOR_L8);
	mImageSetPixel(image, 0, 0, 0xFF000000);
	mImageSetPixel(image, 1, 0, 0xFF555555);
	mImageSetPixel(image, 0, 1, 0xFFAAAAAA);
	mImageSetPixel(image, 1, 1, 0xFFFFFFFF);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0xFF000000);
	assert_int_equal(mImageGetPixel(image, 1, 0), 0xFF555555);
	assert_int_equal(mImageGetPixel(image, 0, 1), 0xFFAAAAAA);
	assert_int_equal(mImageGetPixel(image, 1, 1), 0xFFFFFFFF);

	struct VFile* vf = VFileMemChunk(NULL, 0);
	assert_true(mImageSaveVF(image, vf, "png"));
	mImageDestroy(image);

	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);
	assert_true(isPNG(vf));
	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);

	image = mImageLoadVF(vf);
	vf->close(vf);
	assert_non_null(image);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0xFF000000);
	assert_int_equal(mImageGetPixel(image, 1, 0), 0xFF555555);
	assert_int_equal(mImageGetPixel(image, 0, 1), 0xFFAAAAAA);
	assert_int_equal(mImageGetPixel(image, 1, 1), 0xFFFFFFFF);
	mImageDestroy(image);
}

M_TEST_DEFINE(savePngPal8) {
	struct mImage* image = mImageCreate(2, 2, mCOLOR_PAL8);
	mImageSetPaletteSize(image, 4);
	mImageSetPaletteEntry(image, 0, 0x00000000);
	mImageSetPaletteEntry(image, 1, 0x40FF0000);
	mImageSetPaletteEntry(image, 2, 0x8000FF00);
	mImageSetPaletteEntry(image, 3, 0xC00000FF);

	mImageSetPixelRaw(image, 0, 0, 0);
	mImageSetPixelRaw(image, 1, 0, 1);
	mImageSetPixelRaw(image, 0, 1, 2);
	mImageSetPixelRaw(image, 1, 1, 3);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0x00000000);
	assert_int_equal(mImageGetPixel(image, 1, 0), 0x40FF0000);
	assert_int_equal(mImageGetPixel(image, 0, 1), 0x8000FF00);
	assert_int_equal(mImageGetPixel(image, 1, 1), 0xC00000FF);

	struct VFile* vf = VFileMemChunk(NULL, 0);
	assert_true(mImageSaveVF(image, vf, "png"));
	mImageDestroy(image);

	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);
	assert_true(isPNG(vf));
	assert_int_equal(vf->seek(vf, 0, SEEK_SET), 0);

	image = mImageLoadVF(vf);
	vf->close(vf);
	assert_non_null(image);
	assert_int_equal(image->format, mCOLOR_PAL8);
	assert_int_equal(mImageGetPixelRaw(image, 0, 0), 0);
	assert_int_equal(mImageGetPixelRaw(image, 1, 0), 1);
	assert_int_equal(mImageGetPixelRaw(image, 0, 1), 2);
	assert_int_equal(mImageGetPixelRaw(image, 1, 1), 3);
	assert_int_equal(mImageGetPixel(image, 0, 0), 0x00000000);
	assert_int_equal(mImageGetPixel(image, 1, 0), 0x40FF0000);
	assert_int_equal(mImageGetPixel(image, 0, 1), 0x8000FF00);
	assert_int_equal(mImageGetPixel(image, 1, 1), 0xC00000FF);
	mImageDestroy(image);
}
#endif

M_TEST_DEFINE(convert1x1) {
	const enum mColorFormat formats[] = {
		mCOLOR_XBGR8, mCOLOR_XRGB8,
		mCOLOR_BGRX8, mCOLOR_RGBX8,
		mCOLOR_ABGR8, mCOLOR_ARGB8,
		mCOLOR_BGRA8, mCOLOR_RGBA8,
		mCOLOR_RGB5, mCOLOR_BGR5,
		mCOLOR_RGB565, mCOLOR_BGR565,
		mCOLOR_ARGB5, mCOLOR_ABGR5,
		mCOLOR_RGBA5, mCOLOR_BGRA5,
		mCOLOR_RGB8, mCOLOR_BGR8,
		mCOLOR_L8,
		0
	};

	int i, j;
	for (i = 0; formats[i]; ++i) {
		for (j = 0; formats[j]; ++j) {
			struct mImage* src = mImageCreate(1, 1, formats[i]);
			mImageSetPixel(src, 0, 0, 0xFF181818);

			struct mImage* dst = mImageConvertToFormat(src, formats[j]);
			assert_non_null(dst);
			assert_int_equal(dst->format, formats[j]);
			assert_int_equal(mImageGetPixel(dst, 0, 0), 0xFF181818);

			mImageDestroy(src);
			mImageDestroy(dst);
		}
	}
}

M_TEST_DEFINE(convert2x1) {
	const enum mColorFormat formats[] = {
		mCOLOR_XBGR8, mCOLOR_XRGB8,
		mCOLOR_BGRX8, mCOLOR_RGBX8,
		mCOLOR_ABGR8, mCOLOR_ARGB8,
		mCOLOR_BGRA8, mCOLOR_RGBA8,
		mCOLOR_RGB5, mCOLOR_BGR5,
		mCOLOR_RGB565, mCOLOR_BGR565,
		mCOLOR_ARGB5, mCOLOR_ABGR5,
		mCOLOR_RGBA5, mCOLOR_BGRA5,
		mCOLOR_RGB8, mCOLOR_BGR8,
		mCOLOR_L8,
		0
	};

	int i, j;
	for (i = 0; formats[i]; ++i) {
		for (j = 0; formats[j]; ++j) {
			struct mImage* src = mImageCreate(2, 1, formats[i]);
			mImageSetPixel(src, 0, 0, 0xFF181818);
			mImageSetPixel(src, 1, 0, 0xFF101010);

			struct mImage* dst = mImageConvertToFormat(src, formats[j]);
			assert_non_null(dst);
			assert_int_equal(dst->format, formats[j]);
			assert_int_equal(mImageGetPixel(dst, 0, 0), 0xFF181818);
			assert_int_equal(mImageGetPixel(dst, 1, 0), 0xFF101010);

			mImageDestroy(src);
			mImageDestroy(dst);
		}
	}
}

M_TEST_DEFINE(convert1x2) {
	const enum mColorFormat formats[] = {
		mCOLOR_XBGR8, mCOLOR_XRGB8,
		mCOLOR_BGRX8, mCOLOR_RGBX8,
		mCOLOR_ABGR8, mCOLOR_ARGB8,
		mCOLOR_BGRA8, mCOLOR_RGBA8,
		mCOLOR_RGB5, mCOLOR_BGR5,
		mCOLOR_RGB565, mCOLOR_BGR565,
		mCOLOR_ARGB5, mCOLOR_ABGR5,
		mCOLOR_RGBA5, mCOLOR_BGRA5,
		mCOLOR_RGB8, mCOLOR_BGR8,
		mCOLOR_L8,
		0
	};

	int i, j;
	for (i = 0; formats[i]; ++i) {
		for (j = 0; formats[j]; ++j) {
			struct mImage* src = calloc(1, sizeof(*src));
			src->width = 1;
			src->height = 2;
			src->stride = 8; // Use an unusual stride to make sure the right parts get copied
			src->format = formats[i];
			src->depth = mColorFormatBytes(src->format);
			src->data = calloc(src->stride * src->depth, src->height);
			mImageSetPixel(src, 0, 0, 0xFF181818);
			mImageSetPixel(src, 0, 1, 0xFF101010);

			struct mImage* dst = mImageConvertToFormat(src, formats[j]);
			assert_non_null(dst);
			assert_int_equal(dst->format, formats[j]);
			assert_int_equal(mImageGetPixel(dst, 0, 0), 0xFF181818);
			assert_int_equal(mImageGetPixel(dst, 0, 1), 0xFF101010);

			mImageDestroy(src);
			mImageDestroy(dst);
		}
	}
}

M_TEST_DEFINE(convert2x2) {
	const enum mColorFormat formats[] = {
		mCOLOR_XBGR8, mCOLOR_XRGB8,
		mCOLOR_BGRX8, mCOLOR_RGBX8,
		mCOLOR_ABGR8, mCOLOR_ARGB8,
		mCOLOR_BGRA8, mCOLOR_RGBA8,
		mCOLOR_RGB5, mCOLOR_BGR5,
		mCOLOR_RGB565, mCOLOR_BGR565,
		mCOLOR_ARGB5, mCOLOR_ABGR5,
		mCOLOR_RGBA5, mCOLOR_BGRA5,
		mCOLOR_RGB8, mCOLOR_BGR8,
		mCOLOR_L8,
		0
	};

	int i, j;
	for (i = 0; formats[i]; ++i) {
		for (j = 0; formats[j]; ++j) {
			struct mImage* src = calloc(1, sizeof(*src));
			src->width = 2;
			src->height = 2;
			src->stride = 8; // Use an unusual stride to make sure the right parts get copied
			src->format = formats[i];
			src->depth = mColorFormatBytes(src->format);
			src->data = calloc(src->stride * src->depth, src->height);
			mImageSetPixel(src, 0, 0, 0xFF181818);
			mImageSetPixel(src, 0, 1, 0xFF101010);
			mImageSetPixel(src, 1, 0, 0xFF000000);
			mImageSetPixel(src, 1, 1, 0xFF080808);

			struct mImage* dst = mImageConvertToFormat(src, formats[j]);
			assert_non_null(dst);
			assert_int_equal(dst->format, formats[j]);
			assert_int_equal(mImageGetPixel(dst, 0, 0), 0xFF181818);
			assert_int_equal(mImageGetPixel(dst, 0, 1), 0xFF101010);
			assert_int_equal(mImageGetPixel(dst, 1, 0), 0xFF000000);
			assert_int_equal(mImageGetPixel(dst, 1, 1), 0xFF080808);

			mImageDestroy(src);
			mImageDestroy(dst);
		}
	}
}

M_TEST_DEFINE(blitBoundaries) {
	static const uint32_t spriteBuffer[4] = {
		0xFF000F00, 0xFF000F01,
		0xFF000F10, 0xFF000F11
	};
	static const uint32_t canvasBuffer[9] = {
		0xFF000000, 0xFF000000, 0xFF000000,
		0xFF000000, 0xFF000000, 0xFF000000,
		0xFF000000, 0xFF000000, 0xFF000000
	};

	struct mImage* sprite = mImageCreateFromConstBuffer(2, 2, 2, mCOLOR_XRGB8, spriteBuffer);
	struct mImage* canvas;

#define COMPARE(AA, BA, CA, AB, BB, CB, AC, BC, CC) \
	assert_int_equal(mImageGetPixel(canvas, 0, 0), 0xFF000000 | (AA)); \
	assert_int_equal(mImageGetPixel(canvas, 1, 0), 0xFF000000 | (BA)); \
	assert_int_equal(mImageGetPixel(canvas, 2, 0), 0xFF000000 | (CA)); \
	assert_int_equal(mImageGetPixel(canvas, 0, 1), 0xFF000000 | (AB)); \
	assert_int_equal(mImageGetPixel(canvas, 1, 1), 0xFF000000 | (BB)); \
	assert_int_equal(mImageGetPixel(canvas, 2, 1), 0xFF000000 | (CB)); \
	assert_int_equal(mImageGetPixel(canvas, 0, 2), 0xFF000000 | (AC)); \
	assert_int_equal(mImageGetPixel(canvas, 1, 2), 0xFF000000 | (BC)); \
	assert_int_equal(mImageGetPixel(canvas, 2, 2), 0xFF000000 | (CC))

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -2, -2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -1, -2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 0, -2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 1, -2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 2, -2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 3, -2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -2, -1);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -1, -1);
	COMPARE(0xF11, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 0, -1);
	COMPARE(0xF10, 0xF11, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 1, -1);
	COMPARE(0x000, 0xF10, 0xF11,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 2, -1);
	COMPARE(0x000, 0x000, 0xF10,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 3, -1);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -2, 0);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -1, 0);
	COMPARE(0xF01, 0x000, 0x000,
	        0xF11, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 0, 0);
	COMPARE(0xF00, 0xF01, 0x000,
	        0xF10, 0xF11, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 1, 0);
	COMPARE(0x000, 0xF00, 0xF01,
	        0x000, 0xF10, 0xF11,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 2, 0);
	COMPARE(0x000, 0x000, 0xF00,
	        0x000, 0x000, 0xF10,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 3, 0);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -2, 1);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -1, 1);
	COMPARE(0x000, 0x000, 0x000,
	        0xF01, 0x000, 0x000,
	        0xF11, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 0, 1);
	COMPARE(0x000, 0x000, 0x000,
	        0xF00, 0xF01, 0x000,
	        0xF10, 0xF11, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 1, 1);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0xF00, 0xF01,
	        0x000, 0xF10, 0xF11);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 2, 1);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0xF00,
	        0x000, 0x000, 0xF10);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 3, 1);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -2, 2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -1, 2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0xF01, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 0, 2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0xF00, 0xF01, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 1, 2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0xF00, 0xF01);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 2, 2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0xF00);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 3, 2);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -2, 3);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, -1, 3);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 0, 3);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 1, 3);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 2, 3);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

	canvas = mImageCreateFromConstBuffer(3, 3, 3, mCOLOR_XRGB8, canvasBuffer);
	mImageBlit(canvas, sprite, 3, 3);
	COMPARE(0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000,
	        0x000, 0x000, 0x000);
	mImageDestroy(canvas);

#undef COMPARE
	mImageDestroy(sprite);
}

M_TEST_SUITE_DEFINE(Image,
	cmocka_unit_test(zeroDim),
	cmocka_unit_test(pitchRead),
	cmocka_unit_test(strideRead),
	cmocka_unit_test(oobRead),
	cmocka_unit_test(pitchWrite),
	cmocka_unit_test(strideWrite),
	cmocka_unit_test(oobWrite),
	cmocka_unit_test(paletteAccess),
#ifdef USE_PNG
	cmocka_unit_test(loadPng24),
	cmocka_unit_test(loadPng32),
	cmocka_unit_test(loadPngPalette),
	cmocka_unit_test(savePngNative),
	cmocka_unit_test(savePngNonNative),
	cmocka_unit_test(savePngRoundTrip),
	cmocka_unit_test(savePngL8),
	cmocka_unit_test(savePngPal8),
#endif
	cmocka_unit_test(convert1x1),
	cmocka_unit_test(convert2x1),
	cmocka_unit_test(convert1x2),
	cmocka_unit_test(convert2x2),
	cmocka_unit_test(blitBoundaries),
)
