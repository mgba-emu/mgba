/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_SCRIPT_CONSOLE_H
#define M_SCRIPT_CONSOLE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/script/context.h>
#include <mgba/script/macros.h>
#include <mgba/script/types.h>

struct mCore;
struct mScriptTextBuffer;
mSCRIPT_DECLARE_STRUCT(mCore);
mSCRIPT_DECLARE_STRUCT(mLogger);
mSCRIPT_DECLARE_STRUCT(mScriptConsole);
mSCRIPT_DECLARE_STRUCT(mScriptTextBuffer);

struct mScriptTextBuffer {
	void (*init)(struct mScriptTextBuffer*, const char* name);
	void (*deinit)(struct mScriptTextBuffer*);

	void (*setName)(struct mScriptTextBuffer*, const char* text);

	uint32_t (*getX)(const struct mScriptTextBuffer*);
	uint32_t (*getY)(const struct mScriptTextBuffer*);
	uint32_t (*cols)(const struct mScriptTextBuffer*);
	uint32_t (*rows)(const struct mScriptTextBuffer*);

	void (*print)(struct mScriptTextBuffer*, const char* text);
	void (*clear)(struct mScriptTextBuffer*);
	void (*setSize)(struct mScriptTextBuffer*, uint32_t cols, uint32_t rows);
	void (*moveCursor)(struct mScriptTextBuffer*, uint32_t x, uint32_t y);
	void (*advance)(struct mScriptTextBuffer*, int32_t);
};

struct mLogger;
void mScriptContextAttachLogger(struct mScriptContext*, struct mLogger*);
void mScriptContextDetachLogger(struct mScriptContext*);

typedef struct mScriptTextBuffer* (*mScriptContextBufferFactory)(void*);
void mScriptContextSetTextBufferFactory(struct mScriptContext*, mScriptContextBufferFactory factory, void* cbContext);

CXX_GUARD_END

#endif
