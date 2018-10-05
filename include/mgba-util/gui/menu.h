/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_MENU_H
#define GUI_MENU_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/vector.h>

#define GUI_V_V (struct GUIVariant) { .type = GUI_VARIANT_VOID }
#define GUI_V_U(U) (struct GUIVariant) { .type = GUI_VARIANT_UNSIGNED, .v.u = (U) }
#define GUI_V_I(I) (struct GUIVariant) { .type = GUI_VARIANT_INT, .v.i = (I) }
#define GUI_V_F(F) (struct GUIVariant) { .type = GUI_VARIANT_FLOAT, .v.f = (F) }
#define GUI_V_S(S) (struct GUIVariant) { .type = GUI_VARIANT_STRING, .v.s = (S) }

enum GUIVariantType {
	GUI_VARIANT_VOID = 0,
	GUI_VARIANT_UNSIGNED,
	GUI_VARIANT_INT,
	GUI_VARIANT_FLOAT,
	GUI_VARIANT_STRING
};

struct GUIVariant {
	enum GUIVariantType type;
	union {
		unsigned u;
		int i;
		float f;
		const char* s;
	} v;
};

struct GUIMenu;
struct GUIMenuItem {
	const char* title;
	void* data;
	unsigned state;
	const char* const* validStates;
	const struct GUIVariant* stateMappings;
	unsigned nStates;
	struct GUIMenu* submenu;
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

enum GUIMenuExitReason {
	GUI_MENU_EXIT_ACCEPT,
	GUI_MENU_EXIT_BACK,
	GUI_MENU_EXIT_CANCEL,
};

enum GUIMessageBoxButtons {
	GUI_MESSAGE_BOX_OK = 1,
	GUI_MESSAGE_BOX_CANCEL = 2
};

struct GUIParams;
enum GUIMenuExitReason GUIShowMenu(struct GUIParams* params, struct GUIMenu* menu, struct GUIMenuItem** item);

ATTRIBUTE_FORMAT(printf, 4, 5)
enum GUIMenuExitReason GUIShowMessageBox(struct GUIParams* params, int buttons, int frames, const char* format, ...);

void GUIDrawBattery(struct GUIParams* params);
void GUIDrawClock(struct GUIParams* params);

CXX_GUARD_END

#endif
