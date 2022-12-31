/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_TEST_H
#define M_SCRIPT_TEST_H

#define LOAD_PROGRAM(PROG) \
	do { \
		struct VFile* vf = VFileFromConstMemory(PROG, strlen(PROG)); \
		assert_true(lua->load(lua, NULL, vf)); \
		vf->close(vf); \
	} while(0)

#define TEST_VALUE(TYPE, NAME, VALUE) \
	do { \
		struct mScriptValue val = mSCRIPT_MAKE(TYPE, VALUE); \
		struct mScriptValue* global = lua->getGlobal(lua, NAME); \
		assert_non_null(global); \
		assert_true(global->type->equal(global, &val)); \
		mScriptValueDeref(global); \
	} while(0)

#define TEST_PROGRAM(PROG) \
	LOAD_PROGRAM(PROG); \
	assert_true(lua->run(lua)); \

#endif
