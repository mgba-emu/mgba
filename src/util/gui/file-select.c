/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/gui/file-select.h>

#include <mgba-util/gui/font.h>
#include <mgba-util/gui/menu.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#include <stdlib.h>

#define ITERATION_SIZE 5
#define SCANNING_THRESHOLD_1 50
#ifdef __3DS__
// 3DS is slooooow at opening files
#define SCANNING_THRESHOLD_2 10
#else
#define SCANNING_THRESHOLD_2 50
#endif

static void _cleanFiles(struct GUIMenuItemList* currentFiles) {
	size_t size = GUIMenuItemListSize(currentFiles);
	size_t i;
	for (i = 1; i < size; ++i) {
		free((char*) GUIMenuItemListGetPointer(currentFiles, i)->title);
	}
	GUIMenuItemListClear(currentFiles);
}

static void _upDirectory(char* currentPath) {
	char* end = strrchr(currentPath, '/');
	if (!end) {
		currentPath[0] = '\0';
		return;
	}
	if (!end[1]) {
		// Trailing slash
		end[0] = '\0';
		return _upDirectory(currentPath);
	}
	end[1] = '\0';
}

static int _strpcmp(const void* a, const void* b) {
	return strcasecmp(((const struct GUIMenuItem*) a)->title, ((const struct GUIMenuItem*) b)->title);
}

static bool _refreshDirectory(struct GUIParams* params, const char* currentPath, struct GUIMenuItemList* currentFiles, bool (*filterName)(const char* name), bool (*filterContents)(struct VFile*), const char* preselect) {
	_cleanFiles(currentFiles);

	struct VDir* dir = VDirOpen(currentPath);
	if (!dir) {
		return false;
	}
	*GUIMenuItemListAppend(currentFiles) = (struct GUIMenuItem) { .title = "(Up)" };
	size_t i = 0;
	size_t items = 0;
	struct VDirEntry* de;
	while ((de = dir->listNext(dir))) {
		++i;
		if (!(i % SCANNING_THRESHOLD_1)) {
			uint32_t input = 0;
			GUIPollInput(params, &input, 0);
			if (input & (1 << GUI_INPUT_CANCEL)) {
				dir->close(dir);
				return false;
			}

			params->drawStart();
			if (params->guiPrepare) {
				params->guiPrepare();
			}
			GUIFontPrintf(params->font, 0, GUIFontHeight(params->font), GUI_ALIGN_LEFT, 0xFFFFFFFF, "(scanning for items: %"PRIz"u)", i);
			GUIFontPrintf(params->font, 0, GUIFontHeight(params->font) * 2, GUI_ALIGN_LEFT, 0xFFFFFFFF, "%s", currentPath);
			if (params->guiFinish) {
				params->guiFinish();
			}
			params->drawEnd();
		}
		const char* name = de->name(de);
		if (name[0] == '.') {
			continue;
		}
		if (de->type(de) == VFS_DIRECTORY) {
			size_t len = strlen(name) + 2;
			char* n2 = malloc(len);
			snprintf(n2, len, "%s/", name);
			name = n2;
		} else if (filterName && !filterName(name)) {
			continue;
		} else {
			name = strdup(name);
		}
		*GUIMenuItemListAppend(currentFiles) = (struct GUIMenuItem) { .title = name, .data = GUI_V_U(de->type(de)) };
		++items;
	}
	qsort(GUIMenuItemListGetPointer(currentFiles, 1), GUIMenuItemListSize(currentFiles) - 1, sizeof(struct GUIMenuItem), _strpcmp);
	if (preselect || filterContents) {
		i = 0;
		size_t item = 0;
		while (item < GUIMenuItemListSize(currentFiles)) {
			++i;
			// If we're not filtering the contents, this loop is fast, so there's no need to show updates
			if (filterContents && !(i % SCANNING_THRESHOLD_2)) {
				uint32_t input = 0;
				GUIPollInput(params, &input, 0);
				if (input & (1 << GUI_INPUT_CANCEL)) {
					dir->close(dir);
					return false;
				}

				params->drawStart();
				if (params->guiPrepare) {
					params->guiPrepare();
				}
				GUIFontPrintf(params->font, 0, GUIFontHeight(params->font), GUI_ALIGN_LEFT, 0xFFFFFFFF, "(scanning item %"PRIz"u of %"PRIz"u)", i, items);
				GUIFontPrintf(params->font, 0, GUIFontHeight(params->font) * 2, GUI_ALIGN_LEFT, 0xFFFFFFFF, "%s", currentPath);
				if (params->guiFinish) {
					params->guiFinish();
				}
				params->drawEnd();
			}
			struct GUIMenuItem* testItem = GUIMenuItemListGetPointer(currentFiles, item);
			if (!GUIVariantCompareUInt(testItem->data, VFS_FILE)) {
				++item;
				continue;
			}
			bool failed = false;
			if (filterContents) {
				struct VFile* vf = dir->openFile(dir, testItem->title, O_RDONLY);
				if (!vf) {
					failed = true;
				} else {
					if (!filterContents(vf)) {
						failed = true;
					}
					vf->close(vf);
				}
			}

			if (failed) {
				free((char*) testItem->title);
				GUIMenuItemListShift(currentFiles, item, 1);
			} else {
				if (preselect && strncmp(testItem->title, preselect, PATH_MAX) == 0) {
					params->fileIndex = item;
				}
				++item;
			}
		}
	}
	dir->close(dir);

	return true;
}

bool GUISelectFile(struct GUIParams* params, char* outPath, size_t outLen, bool (*filterName)(const char* name), bool (*filterContents)(struct VFile*), const char* preselect) {
	struct GUIMenu menu = {
		.title = "Select file",
		.subtitle = params->currentPath,
	};
	GUIMenuItemListInit(&menu.items, 0);
	_refreshDirectory(params, params->currentPath, &menu.items, filterName, filterContents, preselect);
	menu.index = params->fileIndex;

	while (true) {
		struct GUIMenuItem* item;
		enum GUIMenuExitReason reason = GUIShowMenu(params, &menu, &item);
		params->fileIndex = menu.index;
		if (reason == GUI_MENU_EXIT_CANCEL) {
			break;
		}
		if (reason == GUI_MENU_EXIT_ACCEPT) {
			if (params->fileIndex == 0) {
				if (strncmp(params->currentPath, params->basePath, PATH_MAX) == 0) {
					continue;
				}
				_upDirectory(params->currentPath);
				if (!_refreshDirectory(params, params->currentPath, &menu.items, filterName, filterContents, NULL)) {
					break;
				}
			} else {
				size_t len = strlen(params->currentPath);
				const char* sep = PATH_SEP;
				if (!len || params->currentPath[len - 1] == *sep) {
					sep = "";
				}
				snprintf(outPath, outLen, "%s%s%s", params->currentPath, sep, item->title);

				struct GUIMenuItemList newFiles;
				GUIMenuItemListInit(&newFiles, 0);
				if (!_refreshDirectory(params, outPath, &newFiles, filterName, filterContents, NULL)) {
					_cleanFiles(&newFiles);
					GUIMenuItemListDeinit(&newFiles);
					_cleanFiles(&menu.items);
					GUIMenuItemListDeinit(&menu.items);
					return true;
				} else {
					_cleanFiles(&menu.items);
					GUIMenuItemListDeinit(&menu.items);
					menu.items = newFiles;
					strlcpy(params->currentPath, outPath, PATH_MAX);
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
			if (!_refreshDirectory(params, params->currentPath, &menu.items, filterName, filterContents, NULL)) {
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
