/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/input.h>

#include <mgba-util/string.h>
#include <mgba-util/table.h>

static const char* eventNames[mSCRIPT_EV_TYPE_MAX] = {
	[mSCRIPT_EV_TYPE_KEY] = "key",
	[mSCRIPT_EV_TYPE_MOUSE_BUTTON] = "mouseButton",
	[mSCRIPT_EV_TYPE_MOUSE_MOVE] = "mouseMove",
	[mSCRIPT_EV_TYPE_MOUSE_WHEEL] = "mouseWheel",
	[mSCRIPT_EV_TYPE_GAMEPAD_BUTTON] = "gamepadButton",
	[mSCRIPT_EV_TYPE_GAMEPAD_HAT] = "gamepadHat",
	[mSCRIPT_EV_TYPE_TRIGGER] = "trigger",
};

static const struct mScriptType* eventTypes[mSCRIPT_EV_TYPE_MAX] = {
	[mSCRIPT_EV_TYPE_KEY] = mSCRIPT_TYPE_MS_S(mScriptKeyEvent),
	[mSCRIPT_EV_TYPE_MOUSE_BUTTON] = mSCRIPT_TYPE_MS_S(mScriptMouseButtonEvent),
	[mSCRIPT_EV_TYPE_MOUSE_MOVE] = mSCRIPT_TYPE_MS_S(mScriptMouseMoveEvent),
	[mSCRIPT_EV_TYPE_MOUSE_WHEEL] = mSCRIPT_TYPE_MS_S(mScriptMouseWheelEvent),
	[mSCRIPT_EV_TYPE_GAMEPAD_BUTTON] = mSCRIPT_TYPE_MS_S(mScriptGamepadButtonEvent),
	[mSCRIPT_EV_TYPE_GAMEPAD_HAT] = mSCRIPT_TYPE_MS_S(mScriptGamepadHatEvent),
};

struct mScriptInputContext {
	struct Table activeKeys;
};

static void _mScriptInputDeinit(struct mScriptInputContext*);
static bool _mScriptInputIsKeyActive(const struct mScriptInputContext*, struct mScriptValue*);

mSCRIPT_DECLARE_STRUCT(mScriptInputContext);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptInputContext, _deinit, _mScriptInputDeinit, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mScriptInputContext, BOOL, isKeyActive, _mScriptInputIsKeyActive, 1, WRAPPER, key);

mSCRIPT_DEFINE_STRUCT(mScriptInputContext)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptInputContext)
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptInputContext, isKeyActive)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptEvent)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptEvent, S32, type)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptEvent, U64, seq)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptKeyEvent)
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptKeyEvent, U8, state)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptKeyEvent, S16, modifiers)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptKeyEvent, S32, key)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptMouseButtonEvent)
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseButtonEvent, U8, mouse)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseButtonEvent, U8, state)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseButtonEvent, U8, button)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptMouseMoveEvent)
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseMoveEvent, U8, mouse)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseMoveEvent, S32, x)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseMoveEvent, S32, y)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptMouseWheelEvent)
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseWheelEvent, U8, mouse)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseWheelEvent, S32, x)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseWheelEvent, S32, y)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptGamepadButtonEvent)
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadButtonEvent, U8, state)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadButtonEvent, U8, pad)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadButtonEvent, U16, button)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptGamepadHatEvent)
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadHatEvent, U8, pad)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadHatEvent, U8, hat)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadHatEvent, U8, direction)
mSCRIPT_DEFINE_END;

void mScriptContextAttachInput(struct mScriptContext* context) {
	struct mScriptInputContext* inputContext = calloc(1, sizeof(*inputContext));
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptInputContext));
	value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	value->value.opaque = inputContext;

	TableInit(&inputContext->activeKeys, 0, NULL);

	mScriptContextSetGlobal(context, "input", value);
	mScriptContextSetDocstring(context, "input", "Singleton instance of struct::mScriptInputContext");

	mScriptContextExportConstants(context, "EV_TYPE", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_EV_TYPE, NONE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_EV_TYPE, KEY),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_EV_TYPE, MOUSE_BUTTON),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_EV_TYPE, MOUSE_MOVE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_EV_TYPE, MOUSE_WHEEL),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_EV_TYPE, GAMEPAD_BUTTON),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_EV_TYPE, TRIGGER),
		mSCRIPT_KV_SENTINEL
	});

	mScriptContextExportConstants(context, "INPUT_STATE", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_STATE, UP),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_STATE, DOWN),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_STATE, HELD),
		mSCRIPT_KV_SENTINEL
	});

	mScriptContextExportConstants(context, "INPUT_DIR", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, NONE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, NORTH),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, EAST),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, SOUTH),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, WEST),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, UP),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, RIGHT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, DOWN),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, LEFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, NORTHEAST),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, NORTHWEST),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, SOUTHEAST),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_INPUT_DIR, SOUTHWEST),
		mSCRIPT_KV_SENTINEL
	});

	mScriptContextExportConstants(context, "KMOD", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, NONE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, LSHIFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, RSHIFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, SHIFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, LCONTROL),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, RCONTROL),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, CONTROL),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, LALT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, RALT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, ALT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, LSUPER),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, RSUPER),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, SUPER),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, CAPS_LOCK),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, NUM_LOCK),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KMOD, SCROLL_LOCK),
		mSCRIPT_KV_SENTINEL
	});
}

void _mScriptInputDeinit(struct mScriptInputContext* context) {
	TableDeinit(&context->activeKeys);
}

bool _mScriptInputIsKeyActive(const struct mScriptInputContext* context, struct mScriptValue* value) {
	uint32_t key;
	struct mScriptValue intValue;
	size_t length;
	const char* strbuf;

	switch (value->type->base) {
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_FLOAT:
		if (!mScriptCast(mSCRIPT_TYPE_MS_U32, value, &intValue)) {
			return false;
		}
		key = intValue.value.u32;
		break;
	case mSCRIPT_TYPE_STRING:
		if (value->value.string->length > 1) {
			return false;
		}
		strbuf = value->value.string->buffer;
		length = value->value.string->size;
		key = utf8Char(&strbuf, &length);
		break;
	default:
		return false;
	}

	void* down = TableLookup(&context->activeKeys, key);
	return down != NULL;
}

static bool _updateKeys(struct mScriptContext* context, struct mScriptKeyEvent* event) {
	int offset = 0;
	switch (event->state) {
	case mSCRIPT_INPUT_STATE_UP:
		offset = -1;
		break;
	case mSCRIPT_INPUT_STATE_DOWN:
		offset = 1;
		break;
	default:
		return true;
	}

	struct mScriptValue* input = mScriptContextGetGlobal(context, "input");
	if (!input) {
		return false;
	}
	struct mScriptInputContext* inputContext = input->value.opaque;
	intptr_t value = (intptr_t) TableLookup(&inputContext->activeKeys, event->key);
	value += offset;
	if (value < 1) {
		TableRemove(&inputContext->activeKeys, event->key);
	} else {
		TableInsert(&inputContext->activeKeys, event->key, (void*) value);
	}
	if (offset < 0 && value > 0) {
		return false;
	}
	if (offset > 0 && value != 1) {
		event->state = mSCRIPT_INPUT_STATE_HELD;
	}
	return true;
}

void mScriptContextFireEvent(struct mScriptContext* context, struct mScriptEvent* event) {
	switch (event->type) {
	case mSCRIPT_EV_TYPE_KEY:
		if (!_updateKeys(context, (struct mScriptKeyEvent*) event)) {
			return;
		}
		break;
	case mSCRIPT_EV_TYPE_NONE:
		return;
	}

	struct mScriptList args;
	mScriptListInit(&args, 1);
	struct mScriptValue* value = mScriptListAppend(&args);
	value->type = eventTypes[event->type];
	value->refs = mSCRIPT_VALUE_UNREF;
	value->flags = 0;
	value->value.opaque = event;
	mScriptContextTriggerCallback(context, eventNames[event->type], &args);
	mScriptListDeinit(&args);
}
