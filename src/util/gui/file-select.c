/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "file-select.h"

#include "util/gui/font.h"
#include "util/vector.h"
#include "util/vfs.h"

DECLARE_VECTOR(FileList, char*);
DEFINE_VECTOR(FileList, char*);

#define ITERATION_SIZE 5
#define SCANNING_THRESHOLD 20

static void _cleanFiles(struct FileList* currentFiles) {
	size_t size = FileListSize(currentFiles);
	size_t i;
	for (i = 1; i < size; ++i) {
		free(*FileListGetPointer(currentFiles, i));
	}
	FileListClear(currentFiles);
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

static bool _refreshDirectory(const struct GUIParams* params, const char* currentPath, struct FileList* currentFiles, bool (*filter)(struct VFile*)) {
	_cleanFiles(currentFiles);

	struct VDir* dir = VDirOpen(currentPath);
	if (!dir) {
		return false;
	}
	*FileListAppend(currentFiles) = "(Up)";
	size_t i = 0;
	struct VDirEntry* de;
	while ((de = dir->listNext(dir))) {
		++i;
		if (i == SCANNING_THRESHOLD) {
			params->drawStart();
			GUIFontPrintf(params->font, 0, GUIFontHeight(params->font), GUI_TEXT_LEFT, 0xFFFFFFFF, "%s", currentPath);
			GUIFontPrintf(params->font, 0, GUIFontHeight(params->font) * 2, GUI_TEXT_LEFT, 0xFFFFFFFF, "(scanning)");
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
				*FileListAppend(currentFiles) = strdup(name);
			}
			vf->close(vf);
		} else {
			*FileListAppend(currentFiles) = strdup(name);
		}
	}
	dir->close(dir);
	return true;
}

bool selectFile(const struct GUIParams* params, const char* basePath, char* outPath, char* currentPath, size_t outLen, bool (*filter)(struct VFile*)) {
	if (!currentPath[0]) {
		strncpy(currentPath, basePath, outLen);
	}
	size_t fileIndex = 0;
	size_t start = 0;

	struct FileList currentFiles;
	FileListInit(&currentFiles, 0);
	_refreshDirectory(params, currentPath, &currentFiles, filter);

	int inputHistory[GUI_INPUT_MAX] = { 0 };

	while (true) {
		int input = params->pollInput();
		int newInput = 0;
		for (int i = 0; i < GUI_INPUT_MAX; ++i) {
			if (input & (1 << i)) {
				++inputHistory[i];
			} else {
				inputHistory[i] = -1;
			}
			if (!inputHistory[i] || (inputHistory[i] >= 30 && !(inputHistory[i] % 6))) {
				newInput |= (1 << i);
			}
		}

		if (newInput & (1 << GUI_INPUT_UP) && fileIndex > 0) {
			--fileIndex;
		}
		if (newInput & (1 << GUI_INPUT_DOWN) && fileIndex < FileListSize(&currentFiles) - 1) {
			++fileIndex;
		}
		if (newInput & (1 << GUI_INPUT_LEFT)) {
			if (fileIndex >= ITERATION_SIZE) {
				fileIndex -= ITERATION_SIZE;
			} else {
				fileIndex = 0;
			}
		}
		if (newInput & (1 << GUI_INPUT_RIGHT)) {
			if (fileIndex + ITERATION_SIZE < FileListSize(&currentFiles)) {
				fileIndex += ITERATION_SIZE;
			} else {
				fileIndex = FileListSize(&currentFiles) - 1;
			}
		}

		if (fileIndex < start) {
			start = fileIndex;
		}
		while ((fileIndex - start + 4) * GUIFontHeight(params->font) > params->height) {
			++start;
		}
		if (newInput & (1 << GUI_INPUT_CANCEL)) {
			_cleanFiles(&currentFiles);
			FileListDeinit(&currentFiles);
			return false;
		}
		if (newInput & (1 << GUI_INPUT_SELECT)) {
			if (fileIndex == 0) {
				_upDirectory(currentPath);
				_refreshDirectory(params, currentPath, &currentFiles, filter);
			} else {
				size_t len = strlen(currentPath);
				const char* sep = PATH_SEP;
				if (currentPath[len - 1] == *sep) {
					sep = "";
				}
				snprintf(outPath, outLen, "%s%s%s", currentPath, sep, *FileListGetPointer(&currentFiles, fileIndex));
				if (!_refreshDirectory(params, outPath, &currentFiles, filter)) {
					return true;
				}
				strncpy(currentPath, outPath, outLen);
			}
			fileIndex = 0;
		}
		if (newInput & (1 << GUI_INPUT_BACK)) {
			if (strncmp(currentPath, basePath, outLen) == 0) {
				_cleanFiles(&currentFiles);
				FileListDeinit(&currentFiles);
				return false;
			}
			_upDirectory(currentPath);
			_refreshDirectory(params, currentPath, &currentFiles, filter);
			fileIndex = 0;
		}

		params->drawStart();
		unsigned y = GUIFontHeight(params->font);
		GUIFontPrintf(params->font, 0, y, GUI_TEXT_LEFT, 0xFFFFFFFF, "%s", currentPath);
		y += 2 * GUIFontHeight(params->font);
		size_t i;
		for (i = start; i < FileListSize(&currentFiles); ++i) {
			int color = 0xE0A0A0A0;
			char bullet = ' ';
			if (i == fileIndex) {
				color = 0xFFFFFFFF;
				bullet = '>';
			}
			GUIFontPrintf(params->font, 0, y, GUI_TEXT_LEFT, color, "%c %s", bullet, *FileListGetPointer(&currentFiles, i));
			y += GUIFontHeight(params->font);
			if (y + GUIFontHeight(params->font) > params->height) {
				break;
			}
		}
		y += GUIFontHeight(params->font) * 2;

		params->drawEnd();
	}
}
