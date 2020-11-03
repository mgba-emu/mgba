/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gui-config.h"

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include "feature/gui/gui-runner.h"
#include "feature/gui/remap.h"
#include <mgba/internal/gba/gba.h>
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif
#include <mgba-util/gui/file-select.h>
#include <mgba-util/gui/menu.h>
#include <mgba-util/vfs.h>

#ifndef GUI_MAX_INPUTS
#define GUI_MAX_INPUTS 7
#endif

static bool _biosNamed(const char* name) {
	char ext[PATH_MAX + 1] = {};
	separatePath(name, NULL, NULL, ext);

	if (strstr(name, "bios")) {
		return true;
	}
	if (!strncmp(ext, "bin", PATH_MAX)) {
		return true;
	}
	return false;
}

void mGUIShowConfig(struct mGUIRunner* runner, struct GUIMenuItem* extra, size_t nExtra) {
	struct GUIMenu menu = {
		.title = "Configure",
		.index = 0,
		.background = &runner->background.d
	};
	GUIMenuItemListInit(&menu.items, 0);
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Frameskip",
		.data = "frameskip",
		.submenu = 0,
		.state = 0,
		.validStates = (const char*[]) {
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
		},
		.nStates = 10
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Show framerate",
		.data = "fpsCounter",
		.submenu = 0,
		.state = false,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Autosave state",
		.data = "autosave",
		.submenu = 0,
		.state = true,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Autoload state",
		.data = "autoload",
		.submenu = 0,
		.state = true,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Use BIOS if found",
		.data = "useBios",
		.submenu = 0,
		.state = true,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Select GBA BIOS path",
		.data = "gba.bios",
	};
#ifdef M_CORE_GB
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Select GB BIOS path",
		.data = "gb.bios",
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Select GBC BIOS path",
		.data = "gbc.bios",
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Select SGB BIOS path",
		.data = "sgb.bios",
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Interframe blending",
		.data = "interframeBlending",
		.submenu = 0,
		.state = false,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Enable SGB features",
		.data = "sgb.model",
		.submenu = 0,
		.state = true,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.stateMappings = (const struct GUIVariant[]) {
			GUI_V_S("DMG"),
			GUI_V_S("SGB"),
		},
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Enable SGB borders",
		.data = "sgb.borders",
		.submenu = 0,
		.state = true,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.nStates = 2
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Crop SGB borders",
		.data = "sgb.borderCrop",
		.submenu = 0,
		.state = false,
		.validStates = (const char*[]) {
			"Off", "On"
		},
		.nStates = 2
	};
#endif
	size_t i;
	const char* mapNames[GUI_MAX_INPUTS + 1];
	if (runner->keySources) {
		for (i = 0; runner->keySources[i].id && i < GUI_MAX_INPUTS; ++i) {
			mapNames[i] = runner->keySources[i].name;
		}
		if (i == 1) {
			// Don't display a name if there's only one input source
			i = 0;
		}
		*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
			.title = "Remap controls",
			.data = "*REMAP",
			.state = 0,
			.validStates = i ? mapNames : 0,
			.nStates = i
		};
	}
	for (i = 0; i < nExtra; ++i) {
		*GUIMenuItemListAppend(&menu.items) = extra[i];
	}
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Save",
		.data = "*SAVE",
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		.title = "Cancel",
		.data = 0,
	};
	enum GUIMenuExitReason reason;
	char gbaBiosPath[256] = "";
#ifdef M_CORE_GB
	char gbBiosPath[256] = "";
	char gbcBiosPath[256] = "";
	char sgbBiosPath[256] = "";
#endif

	struct GUIMenuItem* item;
	for (i = 0; i < GUIMenuItemListSize(&menu.items); ++i) {
		item = GUIMenuItemListGetPointer(&menu.items, i);
		if (!item->validStates || !item->data) {
			continue;
		}
		if (item->stateMappings) {
			size_t j;
			for (j = 0; j < item->nStates; ++j) {
				const struct GUIVariant* v = &item->stateMappings[j];
				struct GUIVariant test;
				switch (v->type) {
				case GUI_VARIANT_VOID:
					if (!mCoreConfigGetValue(&runner->config, item->data)) {
						item->state = j;
						break;
					}
					break;
				case GUI_VARIANT_UNSIGNED:
					if (mCoreConfigGetUIntValue(&runner->config, item->data, &test.v.u) && test.v.u == v->v.u) {
						item->state = j;
						break;
					}
					break;
				case GUI_VARIANT_INT:
					if (mCoreConfigGetIntValue(&runner->config, item->data, &test.v.i) && test.v.i == v->v.i) {
						item->state = j;
						break;
					}
					break;
				case GUI_VARIANT_FLOAT:
					if (mCoreConfigGetFloatValue(&runner->config, item->data, &test.v.f) && fabsf(test.v.f - v->v.f) <= 1e-3f) {
						item->state = j;
						break;
					}
					break;
				case GUI_VARIANT_STRING:
					test.v.s = mCoreConfigGetValue(&runner->config, item->data);
					if (test.v.s && strcmp(test.v.s, v->v.s) == 0) {
						item->state = j;
						break;						
					}
					break;
				}
			}
		} else {
			mCoreConfigGetUIntValue(&runner->config, item->data, &item->state);
		}
	}

	while (true) {
		reason = GUIShowMenu(&runner->params, &menu, &item);
		if (reason != GUI_MENU_EXIT_ACCEPT || !item->data) {
			break;
		}
		if (!strcmp(item->data, "*SAVE")) {
			if (gbaBiosPath[0]) {
				mCoreConfigSetValue(&runner->config, "gba.bios", gbaBiosPath);
			}
			if (gbBiosPath[0]) {
				mCoreConfigSetValue(&runner->config, "gb.bios", gbBiosPath);
			}
			if (gbcBiosPath[0]) {
				mCoreConfigSetValue(&runner->config, "gbc.bios", gbcBiosPath);
			}
			if (sgbBiosPath[0]) {
				mCoreConfigSetValue(&runner->config, "sgb.bios", sgbBiosPath);
			}
			for (i = 0; i < GUIMenuItemListSize(&menu.items); ++i) {
				item = GUIMenuItemListGetPointer(&menu.items, i);
				if (!item->validStates || !item->data || ((const char*) item->data)[0] == '*') {
					continue;
				}
				if (item->stateMappings) {
					const struct GUIVariant* v = &item->stateMappings[item->state];
					switch (v->type) {
					case GUI_VARIANT_VOID:
						mCoreConfigSetValue(&runner->config, item->data, NULL);
						break;
					case GUI_VARIANT_UNSIGNED:
						mCoreConfigSetUIntValue(&runner->config, item->data, v->v.u);
						break;
					case GUI_VARIANT_INT:
						mCoreConfigSetUIntValue(&runner->config, item->data, v->v.i);
						break;
					case GUI_VARIANT_FLOAT:
						mCoreConfigSetFloatValue(&runner->config, item->data, v->v.f);
						break;
					case GUI_VARIANT_STRING:
						mCoreConfigSetValue(&runner->config, item->data, v->v.s);
						break;
					}
				} else {
					mCoreConfigSetUIntValue(&runner->config, item->data, item->state);
				}
			}
			if (runner->keySources) {
				size_t i;
				for (i = 0; runner->keySources[i].id; ++i) {
					mInputMapSave(&runner->core->inputMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->config));
					mInputMapSave(&runner->params.keyMap, runner->keySources[i].id, mCoreConfigGetInput(&runner->config));
				}
			}
			mCoreConfigSave(&runner->config);
			mCoreLoadForeignConfig(runner->core, &runner->config);
			break;
		}
		if (!strcmp(item->data, "*REMAP")) {
			mGUIRemapKeys(&runner->params, &runner->core->inputMap, &runner->keySources[item->state]);
			continue;
		}
		if (!strcmp(item->data, "gba.bios")) {
			// TODO: show box if failed
			if (!GUISelectFile(&runner->params, gbaBiosPath, sizeof(gbaBiosPath), _biosNamed, GBAIsBIOS, NULL)) {
				gbaBiosPath[0] = '\0';
			}
			continue;
		}
#ifdef M_CORE_GB
		if (!strcmp(item->data, "gb.bios")) {
			// TODO: show box if failed
			if (!GUISelectFile(&runner->params, gbBiosPath, sizeof(gbBiosPath), _biosNamed, GBIsBIOS, NULL)) {
				gbBiosPath[0] = '\0';
			}
			continue;
		}
		if (!strcmp(item->data, "gbc.bios")) {
			// TODO: show box if failed
			if (!GUISelectFile(&runner->params, gbcBiosPath, sizeof(gbcBiosPath), _biosNamed, GBIsBIOS, NULL)) {
				gbcBiosPath[0] = '\0';
			}
			continue;
		}
		if (!strcmp(item->data, "sgb.bios")) {
			// TODO: show box if failed
			if (!GUISelectFile(&runner->params, sgbBiosPath, sizeof(sgbBiosPath), _biosNamed, GBIsBIOS, NULL)) {
				sgbBiosPath[0] = '\0';
			}
			continue;
		}
#endif
		if (item->validStates) {
			if (item->state < item->nStates - 1) {
				do {
					++item->state;
				} while (!item->validStates[item->state] && item->state < item->nStates - 1);
				if (!item->validStates[item->state]) {
					item->state = 0;
				}
			} else {
				item->state = 0;
			}
		}
	}
}
