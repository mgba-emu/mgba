/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/script/input.h>

#include <mgba/internal/script/types.h>
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
	uint64_t seq;
	struct Table activeKeys;
	struct mScriptGamepad* activeGamepad;
};

static void _mScriptInputDeinit(struct mScriptInputContext*);
static bool _mScriptInputIsKeyActive(const struct mScriptInputContext*, struct mScriptValue*);
static struct mScriptValue* _mScriptInputActiveKeys(const struct mScriptInputContext*);

mSCRIPT_DECLARE_STRUCT(mScriptInputContext);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(mScriptInputContext, _deinit, _mScriptInputDeinit, 0);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mScriptInputContext, BOOL, isKeyActive, _mScriptInputIsKeyActive, 1, WRAPPER, key);
mSCRIPT_DECLARE_STRUCT_C_METHOD(mScriptInputContext, WLIST, activeKeys, _mScriptInputActiveKeys, 0);

mSCRIPT_DEFINE_STRUCT(mScriptInputContext)
	mSCRIPT_DEFINE_STRUCT_DEINIT(mScriptInputContext)
	mSCRIPT_DEFINE_DOCSTRING("Sequence number of the next event to be emitted")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptInputContext, U64, seq)
	mSCRIPT_DEFINE_DOCSTRING("Check if a given keyboard key is currently held. The input can be either the printable character for a key, the numerical Unicode codepoint, or a special value from C.KEY")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptInputContext, isKeyActive)
	mSCRIPT_DEFINE_DOCSTRING("Get a list of the currently active keys. The values are Unicode codepoints or special key values from C.KEY, not strings, so make sure to convert as needed")
	mSCRIPT_DEFINE_STRUCT_METHOD(mScriptInputContext, activeKeys)
	mSCRIPT_DEFINE_DOCSTRING("The currently active gamepad, if any")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptInputContext, PCS(mScriptGamepad), activeGamepad)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptEvent)
	mSCRIPT_DEFINE_CLASS_DOCSTRING(
		"The base class for all event types. Different events have their own subclasses."
	)
	mSCRIPT_DEFINE_DOCSTRING("The type of this event. See C.EV_TYPE for a list of possible types.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptEvent, S32, type)
	mSCRIPT_DEFINE_DOCSTRING("Sequence number of this event. This value increases monotinically.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptEvent, U64, seq)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptKeyEvent)
	mSCRIPT_DEFINE_CLASS_DOCSTRING("A keyboard key event.")
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_DOCSTRING("The state of the key, represented by a C.INPUT_STATE value")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptKeyEvent, U8, state)
	mSCRIPT_DEFINE_DOCSTRING("A bitmask of current modifiers, represented by ORed C.KMOD values")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptKeyEvent, S16, modifiers)
	mSCRIPT_DEFINE_DOCSTRING(
		"The relevant key for this event. For most printable characters, this will be the Unicode "
		"codepoint of the character. Some special values are present as C.KEY as well."
	)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptKeyEvent, S32, key)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptMouseButtonEvent)
	mSCRIPT_DEFINE_CLASS_DOCSTRING("A mouse button event.")
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_DOCSTRING("Which mouse this event pertains to. Currently, this will always be 0.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseButtonEvent, U8, mouse)
	mSCRIPT_DEFINE_DOCSTRING("The state of the button, represented by a C.INPUT_STATE value")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseButtonEvent, U8, state)
	mSCRIPT_DEFINE_DOCSTRING(
		"Which mouse button this event pertains to. Symbolic names for primary (usually left), "
		"secondary (usually right), and middle are in C.MOUSE_BUTTON, and further buttons "
		"are numeric starting from 3."
	)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseButtonEvent, U8, button)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptMouseMoveEvent)
	mSCRIPT_DEFINE_CLASS_DOCSTRING("A mouse movement event.")
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_DOCSTRING("Which mouse this event pertains to. Currently, this will always be 0.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseMoveEvent, U8, mouse)
	mSCRIPT_DEFINE_DOCSTRING(
		"The x coordinate of the mouse in the context of game screen pixels. "
		"This can be out of bounds of the game screen depending on the size of the window in question."
	)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseMoveEvent, S32, x)
	mSCRIPT_DEFINE_DOCSTRING(
		"The y coordinate of the mouse in the context of game screen pixels. "
		"This can be out of bounds of the game screen depending on the size of the window in question."
	)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseMoveEvent, S32, y)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptMouseWheelEvent)
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_DOCSTRING("Which mouse this event pertains to. Currently, this will always be 0.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseWheelEvent, U8, mouse)
	mSCRIPT_DEFINE_DOCSTRING("The amount scrolled horizontally")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseWheelEvent, S32, x)
	mSCRIPT_DEFINE_DOCSTRING("The amount scrolled vertically")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptMouseWheelEvent, S32, y)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptGamepadButtonEvent)
	mSCRIPT_DEFINE_CLASS_DOCSTRING("A gamead button event.")
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_DOCSTRING("The state of the button, represented by a C.INPUT_STATE value")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadButtonEvent, U8, state)
	mSCRIPT_DEFINE_DOCSTRING("Which gamepad this event pertains to. Currently, this will always be 0.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadButtonEvent, U8, pad)
	mSCRIPT_DEFINE_DOCSTRING(
		"Which button this event pertains to. There is currently no guaranteed button mapping, "
		"and it might change between different controllers."
	)
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadButtonEvent, U16, button)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptGamepadHatEvent)
	mSCRIPT_DEFINE_CLASS_DOCSTRING("A gamepad POV hat event.")
	mSCRIPT_DEFINE_INHERIT(mScriptEvent)
	mSCRIPT_DEFINE_DOCSTRING("Which gamepad this event pertains to. Currently, this will always be 0.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadHatEvent, U8, pad)
	mSCRIPT_DEFINE_DOCSTRING("Which hat this event pertains to. For most gamepads this will be 0.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadHatEvent, U8, hat)
	mSCRIPT_DEFINE_DOCSTRING("The current direction of the hat. See C.INPUT_DIR for possible values.")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepadHatEvent, U8, direction)
mSCRIPT_DEFINE_END;

mSCRIPT_DEFINE_STRUCT(mScriptGamepad)
	mSCRIPT_DEFINE_DOCSTRING("The human-readable name of this gamepad")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepad, CHARP, visibleName)
	mSCRIPT_DEFINE_DOCSTRING("The internal name of this gamepad, generally unique to the specific type of gamepad")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepad, CHARP, internalName)
	mSCRIPT_DEFINE_DOCSTRING("An indexed list of the current values of each axis")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepad, LIST, axes)
	mSCRIPT_DEFINE_DOCSTRING("An indexed list of the current values of each button")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepad, LIST, buttons)
	mSCRIPT_DEFINE_DOCSTRING("An indexed list of the current values of POV hat")
	mSCRIPT_DEFINE_STRUCT_MEMBER(mScriptGamepad, LIST, hats)
mSCRIPT_DEFINE_END;

void mScriptContextAttachInput(struct mScriptContext* context) {
	struct mScriptInputContext* inputContext = calloc(1, sizeof(*inputContext));
	struct mScriptValue* value = mScriptValueAlloc(mSCRIPT_TYPE_MS_S(mScriptInputContext));
	value->flags = mSCRIPT_VALUE_FLAG_FREE_BUFFER;
	value->value.opaque = inputContext;

	inputContext->seq = 0;
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

	mScriptContextExportConstants(context, "KEY", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, NONE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, BACKSPACE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, TAB),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, ENTER),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, ESCAPE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, DELETE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F1),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F2),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F3),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F4),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F5),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F6),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F7),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F8),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F9),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F10),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F11),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F12),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F13),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F14),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F15),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F16),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F17),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F18),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F19),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F20),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F21),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F22),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F23),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, F24),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, UP),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, RIGHT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, DOWN),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, LEFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, PAGE_UP),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, PAGE_DOWN),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, HOME),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, END),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, INSERT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, BREAK),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, CLEAR),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, PRINT_SCREEN),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, SYSRQ),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, MENU),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, HELP),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, LSHIFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, RSHIFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, SHIFT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, LCONTROL),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, RCONTROL),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, CONTROL),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, LALT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, RALT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, ALT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, LSUPER),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, RSUPER),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, SUPER),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, CAPS_LOCK),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, NUM_LOCK),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, SCROLL_LOCK),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_0),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_1),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_2),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_3),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_4),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_5),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_6),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_7),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_8),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_9),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_PLUS),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_MINUS),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_MULTIPLY),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_DIVIDE),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_COMMA),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_POINT),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_KEY, KP_ENTER),
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

	mScriptContextExportConstants(context, "MOUSE_BUTTON", (struct mScriptKVPair[]) {
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_MOUSE_BUTTON, PRIMARY),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_MOUSE_BUTTON, SECONDARY),
		mSCRIPT_CONSTANT_PAIR(mSCRIPT_MOUSE_BUTTON, MIDDLE),
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

static struct mScriptValue* _mScriptInputActiveKeys(const struct mScriptInputContext* context) {
	struct mScriptValue* list = mScriptValueAlloc(mSCRIPT_TYPE_MS_LIST);
	struct TableIterator iter;
	if (!TableIteratorStart(&context->activeKeys, &iter)) {
		return list;
	}
	do {
		uint32_t key = TableIteratorGetKey(&context->activeKeys, &iter);
		*mScriptListAppend(list->value.list) = mSCRIPT_MAKE_U32(key);
	} while (TableIteratorNext(&context->activeKeys, &iter));
	return list;
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
	struct mScriptValue* input = mScriptContextGetGlobal(context, "input");
	if (!input) {
		return;
	}
	struct mScriptInputContext* inputContext = input->value.opaque;

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
	event->seq = inputContext->seq;
	++inputContext->seq;
	mScriptContextTriggerCallback(context, eventNames[event->type], &args);
	mScriptListDeinit(&args);
}

void mScriptContextClearKeys(struct mScriptContext* context) {
	struct mScriptValue* input = mScriptContextGetGlobal(context, "input");
	if (!input) {
		return;
	}
	struct mScriptInputContext* inputContext = input->value.opaque;
	size_t keyCount = TableSize(&inputContext->activeKeys);
	uint32_t* keys = calloc(keyCount, sizeof(uint32_t));
	struct TableIterator iter;
	size_t i = 0;
	if (!TableIteratorStart(&inputContext->activeKeys, &iter)) {
		free(keys);
		return;
	}
	do {
		keys[i] = TableIteratorGetKey(&inputContext->activeKeys, &iter);
		++i;
	} while (TableIteratorNext(&inputContext->activeKeys, &iter));

	struct mScriptKeyEvent event = {
		.d = {
			.type = mSCRIPT_EV_TYPE_KEY
		},
		.state = mSCRIPT_INPUT_STATE_UP,
		.modifiers = 0
	};
	for (i = 0; i < keyCount; ++i) {
		event.key = keys[i];
		intptr_t value = (intptr_t) TableLookup(&inputContext->activeKeys, event.key);
		if (value > 1) {
			TableInsert(&inputContext->activeKeys, event.key, (void*) 1);
		}
		mScriptContextFireEvent(context, &event.d);
	}

	free(keys);
}

int mScriptContextGamepadAttach(struct mScriptContext* context, struct mScriptGamepad* pad) {
	struct mScriptValue* input = mScriptContextGetGlobal(context, "input");
	if (!input) {
		return false;
	}
	struct mScriptInputContext* inputContext = input->value.opaque;
	if (inputContext->activeGamepad) {
		return -1;
	}
	inputContext->activeGamepad = pad;

	return 0;
}

bool mScriptContextGamepadDetach(struct mScriptContext* context, int pad) {
	if (pad != 0) {
		return false;
	}
	struct mScriptValue* input = mScriptContextGetGlobal(context, "input");
	if (!input) {
		return false;
	}
	struct mScriptInputContext* inputContext = input->value.opaque;
	if (!inputContext->activeGamepad) {
		return false;
	}
	inputContext->activeGamepad = NULL;
	return true;
}

struct mScriptGamepad* mScriptContextGamepadLookup(struct mScriptContext* context, int pad) {
	if (pad != 0) {
		return NULL;
	}
	struct mScriptValue* input = mScriptContextGetGlobal(context, "input");
	if (!input) {
		return false;
	}
	struct mScriptInputContext* inputContext = input->value.opaque;
	return inputContext->activeGamepad;
}

void mScriptGamepadInit(struct mScriptGamepad* gamepad) {
	memset(gamepad, 0, sizeof(*gamepad));

	mScriptListInit(&gamepad->axes, 8);
	mScriptListInit(&gamepad->buttons, 1);
	mScriptListInit(&gamepad->hats, 1);
}

void mScriptGamepadDeinit(struct mScriptGamepad* gamepad) {
	mScriptListDeinit(&gamepad->axes);
	mScriptListDeinit(&gamepad->buttons);
	mScriptListDeinit(&gamepad->hats);
}

void mScriptGamepadSetAxisCount(struct mScriptGamepad* gamepad, unsigned count) {
	if (count > UINT8_MAX) {
		count = UINT8_MAX;
	}

	unsigned oldSize = mScriptListSize(&gamepad->axes);
	mScriptListResize(&gamepad->axes, (ssize_t) count - oldSize);
	unsigned i;
	for (i = oldSize; i < count; ++i) {
		*mScriptListGetPointer(&gamepad->axes, i) = mSCRIPT_MAKE_S16(0);
	}
}

void mScriptGamepadSetButtonCount(struct mScriptGamepad* gamepad, unsigned count) {
	if (count > UINT16_MAX) {
		count = UINT16_MAX;
	}

	unsigned oldSize = mScriptListSize(&gamepad->buttons);
	mScriptListResize(&gamepad->buttons, (ssize_t) count - oldSize);
	unsigned i;
	for (i = oldSize; i < count; ++i) {
		*mScriptListGetPointer(&gamepad->buttons, i) = mSCRIPT_MAKE_BOOL(false);
	}
}

void mScriptGamepadSetHatCount(struct mScriptGamepad* gamepad, unsigned count) {
	if (count > UINT8_MAX) {
		count = UINT8_MAX;
	}

	unsigned oldSize = mScriptListSize(&gamepad->hats);
	mScriptListResize(&gamepad->hats, (ssize_t) count - oldSize);
	unsigned i;
	for (i = oldSize; i < count; ++i) {
		*mScriptListGetPointer(&gamepad->hats, i) = mSCRIPT_MAKE_U8(0);
	}
}

void mScriptGamepadSetAxis(struct mScriptGamepad* gamepad, unsigned id, int16_t value) {
	if (id >= mScriptListSize(&gamepad->axes)) {
		return;
	}

	mScriptListGetPointer(&gamepad->axes, id)->value.s32 = value;
}

void mScriptGamepadSetButton(struct mScriptGamepad* gamepad, unsigned id, bool down) {
	if (id >= mScriptListSize(&gamepad->buttons)) {
		return;
	}

	mScriptListGetPointer(&gamepad->buttons, id)->value.u32 = down;
}

void mScriptGamepadSetHat(struct mScriptGamepad* gamepad, unsigned id, int direction) {
	if (id >= mScriptListSize(&gamepad->hats)) {
		return;
	}

	mScriptListGetPointer(&gamepad->hats, id)->value.u32 = direction;
}

int16_t mScriptGamepadGetAxis(struct mScriptGamepad* gamepad, unsigned id) {
	if (id >= mScriptListSize(&gamepad->axes)) {
		return 0;
	}

	return mScriptListGetPointer(&gamepad->axes, id)->value.s32;
}

bool mScriptGamepadGetButton(struct mScriptGamepad* gamepad, unsigned id) {
	if (id >= mScriptListSize(&gamepad->buttons)) {
		return false;
	}

	return mScriptListGetPointer(&gamepad->buttons, id)->value.u32;
}

int mScriptGamepadGetHat(struct mScriptGamepad* gamepad, unsigned id) {
	if (id >= mScriptListSize(&gamepad->hats)) {
		return mSCRIPT_INPUT_DIR_NONE;
	}

	return mScriptListGetPointer(&gamepad->hats, id)->value.u32;
}

void mScriptContextGetInputTypes(struct Table* types) {
	mScriptTypeAdd(types, mSCRIPT_TYPE_MS_S(mScriptEvent));
	mScriptTypeAdd(types, mSCRIPT_TYPE_MS_S(mScriptKeyEvent));
	mScriptTypeAdd(types, mSCRIPT_TYPE_MS_S(mScriptMouseMoveEvent));
	mScriptTypeAdd(types, mSCRIPT_TYPE_MS_S(mScriptMouseButtonEvent));
	mScriptTypeAdd(types, mSCRIPT_TYPE_MS_S(mScriptGamepadButtonEvent));
	mScriptTypeAdd(types, mSCRIPT_TYPE_MS_S(mScriptGamepadHatEvent));
	mScriptTypeAdd(types, mSCRIPT_TYPE_MS_S(mScriptGamepad));
}
