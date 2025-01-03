/* Copyright (c) 2013-2024 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/console.h>

#include <mgba/script/context.h>

struct mScriptConsole {
	struct mLogger* logger;
	mScriptContextBufferFactory textBufferFactory;
	void* textBufferContext;
};

static struct mScriptTextBuffer* _mScriptConsoleCreateBuffer(struct mScriptConsole* lib, const char* name) {
	struct mScriptTextBuffer* buffer = lib->textBufferFactory(lib->textBufferContext);
	buffer->init(buffer, name);
	return buffer;
}

static void mScriptConsoleLog(struct mScriptConsole* console, const char* msg) {
	if (console->logger) {
		mLogExplicit(console->logger, _mLOG_CAT_SCRIPT, mLOG_INFO, "%s", msg);
	} else {
		mLog(_mLOG_CAT_SCRIPT, mLOG_INFO, "%s", msg);
	}
}

static void mScriptConsoleWarn(struct mScriptConsole* console, const char* msg) {
	if (console->logger) {
		mLogExplicit(console->logger, _mLOG_CAT_SCRIPT, mLOG_WARN, "%s", msg);
	} else {
		mLog(_mLOG_CAT_SCRIPT, mLOG_WARN, "%s", msg);
	}
}

static void mScriptConsoleError(struct mScriptConsole* console, const char* msg) {
	if (console->logger) {
		mLogExplicit(console->logger, _mLOG_CAT_SCRIPT, mLOG_ERROR, "%s", msg);
	} else {
		mLog(_mLOG_CAT_SCRIPT, mLOG_ERROR, "%s", msg);
	}
}

mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptConsole, log, mScriptConsoleLog, 1, CHARP, msg);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptConsole, warn, mScriptConsoleWarn, 1, CHARP, msg);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptConsole, error, mScriptConsoleError, 1, CHARP, msg);
mSCRIPT_DECLARE_STRUCT_METHOD_WITH_DEFAULTS(mScriptConsole, S(mScriptTextBuffer), createBuffer, _mScriptConsoleCreateBuffer, 1, CHARP, name);

mSCRIPT_DEFINE_STRUCT(mScriptConsole)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"A global singleton object `console` that can be used for presenting textual information to the user via a console."
	)
	mSCRIPT_DEFINE_DOCSTRING("Print a log to the console")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, log)
	mSCRIPT_DEFINE_DOCSTRING("Print a warning to the console")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, warn)
	mSCRIPT_DEFINE_DOCSTRING("Print an error to the console")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, error)
	mSCRIPT_DEFINE_DOCSTRING("Create a text buffer that can be used to display custom information")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptConsole, createBuffer)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT_BINDING_DEFAULTS(mScriptConsole, createBuffer)
	mSCRIPT_CHARP(NULL)
mSCRIPT_DEFINE_DEFAULTS_END;

static struct mScriptConsole* _ensureConsole(struct mScriptContext* context) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "console");
	if (value) {
		return value->value.opaque;
	}
	struct mScriptConsole* console = calloc(1, sizeof(*console));
	value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptConsole));
	value->value.opaque = console;
	value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	mScriptContextSetGlobal(context, "console", value);
	mScriptContextSetDocstring(context, "console", "Singleton instance of struct::mScriptConsole");
	return console;
}

void mScriptContextAttachLogger(struct mScriptContext* context, struct mLogger* logger) {
	struct mScriptConsole* console = _ensureConsole(context);
	console->logger = logger ? logger : mLogGetContext();
}

void mScriptContextDetachLogger(struct mScriptContext* context) {
	struct mScriptValue* value = mScriptContextGetGlobal(context, "console");
	if (!value) {
		return;
	}
	struct mScriptConsole* console = value->value.opaque;
	console->logger = mLogGetContext();
}

mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, deinit, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, getX, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, getY, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, cols, 0);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(mScriptTextBuffer, U32, rows, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, print, 1, CHARP, text);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, clear, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, setSize, 2, U32, cols, U32, rows);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, moveCursor, 2, U32, x, U32, y);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, advance, 1, S32, adv);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(mScriptTextBuffer, setName, 1, CHARP, name);

mSCRIPT_DEFINE_STRUCT(mScriptTextBuffer)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"An object that can be used to present texual data to the user. It is displayed monospaced, "
		"and text can be edited after sending by moving the cursor or clearing the buffer."
	)
	mSCRIPT_DEFINE_STRUCT_DEINIT_NAMED(mScriptTextBuffer, deinit)
	mSCRIPT_DEFINE_DOCSTRING("Get the current x position of the cursor")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, getX)
	mSCRIPT_DEFINE_DOCSTRING("Get the current y position of the cursor")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, getY)
	mSCRIPT_DEFINE_DOCSTRING("Get number of columns in the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, cols)
	mSCRIPT_DEFINE_DOCSTRING("Get number of rows in the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, rows)
	mSCRIPT_DEFINE_DOCSTRING("Print a string to the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, print)
	mSCRIPT_DEFINE_DOCSTRING("Clear the buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, clear)
	mSCRIPT_DEFINE_DOCSTRING("Set the number of rows and columns")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, setSize)
	mSCRIPT_DEFINE_DOCSTRING("Set the position of the cursor")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, moveCursor)
	mSCRIPT_DEFINE_DOCSTRING("Advance the cursor a number of columns")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, advance)
	mSCRIPT_DEFINE_DOCSTRING("Set the user-visible name of this buffer")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptTextBuffer, setName)
mSCRIPT_DEFINE_END;

void mScriptContextSetTextBufferFactory(struct mScriptContext* context, mScriptContextBufferFactory factory, void* cbContext) {
	struct mScriptConsole* console = _ensureConsole(context);
	console->textBufferFactory = factory;
	console->textBufferContext = cbContext;
}
