/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_MENU_H
#define GUI_MENU_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/gui.h>
#include <mgba-util/vector.h>

#define GUI_V_V (struct GUIVariant) { .type = GUI_VARIANT_VOID }
#define GUI_V_U(U) (struct GUIVariant) { .type = GUI_VARIANT_UNSIGNED, .v.u = (U) }
#define GUI_V_I(I) (struct GUIVariant) { .type = GUI_VARIANT_INT, .v.i = (I) }
#define GUI_V_F(F) (struct GUIVariant) { .type = GUI_VARIANT_FLOAT, .v.f = (F) }
#define GUI_V_S(S) (struct GUIVariant) { .type = GUI_VARIANT_STRING, .v.s = (S) }
#define GUI_V_P(P) (struct GUIVariant) { .type = GUI_VARIANT_POINTER, .v.p = (P) }

#define GUIVariantIs(V, T) ((V).type == GUI_VARIANT_##T)
#define GUIVariantIsVoid(V) GUIVariantIs(V, VOID)
#define GUIVariantIsUInt(V) GUIVariantIs(V, UNSIGNED)
#define GUIVariantIsInt(V) GUIVariantIs(V, INT)
#define GUIVariantIsFloat(V) GUIVariantIs(V, FLOAT)
#define GUIVariantIsString(V) GUIVariantIs(V, STRING)
#define GUIVariantIsPointer(V) GUIVariantIs(V, POINTER)

#define GUIVariantCompareUInt(V, X) (GUIVariantIsUInt(V) && (V).v.u == (X))
#define GUIVariantCompareInt(V, X) (GUIVariantIsInt(V) && (V).v.i == (X))
#define GUIVariantCompareString(V, X) (GUIVariantIsString(V) && strcmp((V).v.s, (X)) == 0)

enum GUIVariantType {
	GUI_VARIANT_VOID = 0,
	GUI_VARIANT_UNSIGNED,
	GUI_VARIANT_INT,
	GUI_VARIANT_FLOAT,
	GUI_VARIANT_STRING,
	GUI_VARIANT_POINTER,
};

struct GUIVariant {
	enum GUIVariantType type;
	union {
		unsigned u;
		int i;
		float f;
		const char* s;
		void* p;
	} v;
};

struct GUIMenu;
struct GUIMenuItem {
	const char* title;
	struct GUIVariant data;
	unsigned state;
	const char* const* validStates;
	const struct GUIVariant* stateMappings;
	unsigned nStates;
	struct GUIMenu* submenu;
	bool readonly;
};

DECLARE_VECTOR(GUIMenuItemList, struct GUIMenuItem);

struct GUIBackground;
struct GUIMenu {
	const char* title;
	const char* subtitle;
	struct GUIMenuItemList items;
	size_t index;
	struct GUIBackground* background;
};

struct GUIMenuSavedState {
	struct GUIMenu* menu;
	size_t start;
};

DECLARE_VECTOR(GUIMenuSavedList, struct GUIMenuSavedState);

struct GUIMenuState {
	size_t start;
	int cursorOverItem;
	enum GUICursorState cursor;
	unsigned cx, cy;
	struct GUIMenuSavedList stack;

	struct GUIMenuItem* resultItem;
};

enum GUIMenuExitReason {
	GUI_MENU_CONTINUE = 0,
	GUI_MENU_EXIT_ACCEPT,
	GUI_MENU_EXIT_BACK,
	GUI_MENU_EXIT_CANCEL,
	GUI_MENU_ENTER,
};

enum GUIMessageBoxButtons {
	GUI_MESSAGE_BOX_OK = 1,
	GUI_MESSAGE_BOX_CANCEL = 2
};

struct GUIParams;
void GUIMenuStateInit(struct GUIMenuState*);
void GUIMenuStateDeinit(struct GUIMenuState*);

enum GUIMenuExitReason GUIShowMenu(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuItem** item);
enum GUIMenuExitReason GUIMenuRun(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuState* state);

ATTRIBUTE_FORMAT(printf, 4, 5)
enum GUIMenuExitReason GUIShowMessageBox(struct GUIParams* params, int buttons, int frames, const char* format, ...);

void GUIDrawBattery(struct GUIParams* params);
void GUIDrawClock(struct GUIParams* params);

CXX_GUARD_END

#endif
