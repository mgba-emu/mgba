/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>
#include <mgba/script.h>

#include "script/test.h"

#define SETUP_LUA \
	struct mScriptContext context; \
	mScriptContextInit(&context); \
	struct mScriptEngineContext* lua = mScriptContextRegisterEngine(&context, mSCRIPT_ENGINE_LUA); \
	mScriptContextAttachStdlib(&context); \
	mScriptContextAttachImage(&context)

M_TEST_SUITE_SETUP(mScriptImage) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptImage) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(members) {
	SETUP_LUA;

	TEST_PROGRAM("assert(image)");
	TEST_PROGRAM("assert(image.new)");
	TEST_PROGRAM("assert(image.load)");
	TEST_PROGRAM("im = image.new(1, 1)");
	TEST_PROGRAM("assert(im)");
	TEST_PROGRAM("assert(im.width == 1)");
	TEST_PROGRAM("assert(im.height == 1)");
	TEST_PROGRAM("assert(im.save)");
	TEST_PROGRAM("assert(im.save)");
	TEST_PROGRAM("assert(im.getPixel)");
	TEST_PROGRAM("assert(im.setPixel)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(zeroDim) {
	SETUP_LUA;

	TEST_PROGRAM("im = image.new(0, 0)");
	TEST_PROGRAM("assert(not im)");
	TEST_PROGRAM("im = image.new(1, 0)");
	TEST_PROGRAM("assert(not im)");
	TEST_PROGRAM("im = image.new(0, 1)");
	TEST_PROGRAM("assert(not im)");
	TEST_PROGRAM("im = image.new(1, 1)");
	TEST_PROGRAM("assert(im)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(pixelColorDefault) {
	SETUP_LUA;

	TEST_PROGRAM("im = image.new(1, 1)");
	TEST_PROGRAM("assert(im:getPixel(0, 0) == 0)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(pixelColorRoundTrip) {
	SETUP_LUA;

	TEST_PROGRAM("im = image.new(1, 1)");
	TEST_PROGRAM("im:setPixel(0, 0, 0xFF123456)");
	TEST_PROGRAM("assert(im:getPixel(0, 0) == 0xFF123456)");

	mScriptContextDeinit(&context);
}

#ifdef USE_PNG
M_TEST_DEFINE(saveLoadRoundTrip) {
	SETUP_LUA;

	unlink("tmp.png");
	TEST_PROGRAM("im = image.new(1, 1)");
	TEST_PROGRAM("im:setPixel(0, 0, 0xFF123456)");
	TEST_PROGRAM("assert(im:save('tmp.png'))");
	TEST_PROGRAM("im = image.load('tmp.png')");
	TEST_PROGRAM("assert(im)");
	TEST_PROGRAM("assert(im:getPixel(0, 0) == 0xFF123456)");
	unlink("tmp.png");

	mScriptContextDeinit(&context);
}
#endif

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptImage,
	cmocka_unit_test(members),
	cmocka_unit_test(zeroDim),
	cmocka_unit_test(pixelColorDefault),
	cmocka_unit_test(pixelColorRoundTrip),
#ifdef USE_PNG
	cmocka_unit_test(saveLoadRoundTrip),
#endif
)
