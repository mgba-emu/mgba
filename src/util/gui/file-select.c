/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "file-select.h"

#include "util/gui/font.h"
#include "util/gui/menu.h"
#include "util/vfs.h"

#include <stdlib.h>

#define ITERATION_SIZE 5
#define SCANNING_THRESHOLD 20

static void _cleanFiles(struct GUIMenuItemList* currentFiles) {
	size_t size = GUIMenuItemListSize(currentFiles);
	size_t i;
	for (i = 1; i < size; ++i) {
		free(GUIMenuItemListGetPointer(currentFiles, i)->title);
	}
	GUIMenuItemListClear(currentFiles);
}

static void _upDirectory(char* currentPath) {
	char* end = strrchr(currentPath, '/');
	if (!end) {
		return;
	}
	if (end == currentPath) {
		end[1] = '\0';
		return;
	}
	end[0] = '\0';
	if (end[1]) {
		return;
	}
	// TODO: What if there was a trailing slash?
}

static int _strpcmp(const void* a, const void* b) {
	return strcmp(((const struct GUIMenuItem*) a)->title, ((const struct GUIMenuItem*) b)->title);
}

static bool _refreshDirectory(struct GUIParams* params, const char* currentPath, struct GUIMenuItemList* currentFiles, bool (*filter)(struct VFile*)) {
	_cleanFiles(currentFiles);

	struct VDir* dir = VDirOpen(currentPath);
	if (!dir) {
		return false;
	}
	*GUIMenuItemListAppend(currentFiles) = (struct GUIMenuItem) { .title = "(Up)" };
	size_t i = 0;
	struct VDirEntry* de;
	while ((de = dir->listNext(dir))) {
		++i;
		if (!(i % SCANNING_THRESHOLD)) {
			int input = 0;
			GUIPollInput(params, &input, 0);
			if (input & (1 << GUI_INPUT_CANCEL)) {
				return false;
			}

			params->drawStart();
			if (params->guiPrepare) {
				params->guiPrepare();
			}
			GUIFontPrintf(params->font, 0, GUIFontHeight(params->font), GUI_TEXT_LEFT, 0xFFFFFFFF, "%s", currentPath);
			GUIFontPrintf(params->font, 0, GUIFontHeight(params->font) * 2, GUI_TEXT_LEFT, 0xFFFFFFFF, "(scanning item %lu)", i);
			if (params->guiFinish) {
				params->guiFinish();
			}
			params->drawEnd();
		}
		const char* name = de->name(de);
		if (name[0] == '.') {
			continue;
		}
		if (de->type(de) == VFS_FILE) {
			struct VFile* vf = dir->openFile(dir, name, O_RDONLY);
			if (!vf) {
				continue;
			}
			if (!filter || filter(vf)) {
				*GUIMenuItemListAppend(currentFiles) = (struct GUIMenuItem) { .title = strdup(name) };
			}
			vf->close(vf);
		} else {
			*GUIMenuItemListAppend(currentFiles) = (struct GUIMenuItem) { .title = strdup(name) };
		}
	}
	dir->close(dir);
	qsort(GUIMenuItemListGetPointer(currentFiles, 1), GUIMenuItemListSize(currentFiles) - 1, sizeof(struct GUIMenuItem), _strpcmp);

	return true;
}

bool GUISelectFile(struct GUIParams* params, char* outPath, size_t outLen, bool (*filter)(struct VFile*)) {
	struct GUIMenu menu = {
		.title = params->currentPath,
		.index = params->fileIndex,
	};
	GUIMenuItemListInit(&menu.items, 0);
	_refreshDirectory(params, params->currentPath, &menu.items, filter);

	while (true) {
		struct GUIMenuItem item;
		enum GUIMenuExitReason reason = GUIShowMenu(params, &menu, &item);
		params->fileIndex = menu.index;
		if (reason == GUI_MENU_EXIT_CANCEL) {
			break;
		}
		if (reason == GUI_MENU_EXIT_ACCEPT) {
			if (params->fileIndex == 0) {
				_upDirectory(params->currentPath);
				if (!_refreshDirectory(params, params->currentPath, &menu.items, filter)) {
					break;
				}
			} else {
				size_t len = strlen(params->currentPath);
				const char* sep = PATH_SEP;
				if (params->currentPath[len - 1] == *sep) {
					sep = "";
				}
				snprintf(outPath, outLen, "%s%s%s", params->currentPath, sep, item.title);

				struct GUIMenuItemList newFiles;
				GUIMenuItemListInit(&newFiles, 0);
				if (!_refreshDirectory(params, outPath, &newFiles, filter)) {
					_cleanFiles(&newFiles);
					GUIMenuItemListDeinit(&newFiles);
					struct VFile* vf = VFileOpen(outPath, O_RDONLY);
					if (!vf) {
						continue;
					}
					if (!filter || filter(vf)) {
						vf->close(vf);
						_cleanFiles(&menu.items);
						GUIMenuItemListDeinit(&menu.items);
						return true;
					}
					vf->close(vf);
					break;
				} else {
					_cleanFiles(&menu.items);
					GUIMenuItemListDeinit(&menu.items);
					menu.items = newFiles;
					strncpy(params->currentPath, outPath, PATH_MAX);
				}
			}
			params->fileIndex = 0;
			menu.index = 0;
		}
		if (reason == GUI_MENU_EXIT_BACK) {
			if (strncmp(params->currentPath, params->basePath, PATH_MAX) == 0) {
				break;
			}
			_upDirectory(params->currentPath);
			if (!_refreshDirectory(params, params->currentPath, &menu.items, filter)) {
				break;
			}
			params->fileIndex = 0;
			menu.index = 0;
		}
	}

	_cleanFiles(&menu.items);
	GUIMenuItemListDeinit(&menu.items);
	return false;
}
