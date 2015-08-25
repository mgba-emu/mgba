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

static void _cleanFiles(struct FileList* currentFiles) {
	size_t size = FileListSize(currentFiles);
	size_t i;
	for (i = 0; i < size; ++i) {
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

static bool _refreshDirectory(const char* currentPath, struct FileList* currentFiles) {
	_cleanFiles(currentFiles);

	struct VDir* dir = VDirOpen(currentPath);
	if (!dir) {
		return false;
	}
	struct VDirEntry* de;
	while ((de = dir->listNext(dir))) {
		if (de->name(de)[0] == '.') {
			continue;
		}
		*FileListAppend(currentFiles) = strdup(de->name(de));
	}
	dir->close(dir);
	return true;
}

bool selectFile(const struct GUIParams* params, const char* basePath, char* outPath, size_t outLen, const char* suffix) {
	char currentPath[256];
	strncpy(currentPath, basePath, sizeof(currentPath));
	int oldInput = -1;
	size_t fileIndex = 0;
	size_t start = 0;

	struct FileList currentFiles;
	FileListInit(&currentFiles, 0);
	_refreshDirectory(currentPath, &currentFiles);

	while (true) {
		int input = params->pollInput();
		int newInput = input & (oldInput ^ input);
		oldInput = input;

		if (newInput & (1 << GUI_INPUT_UP) && fileIndex > 0) {
			--fileIndex;
		}
		if (newInput & (1 << GUI_INPUT_DOWN) && fileIndex < FileListSize(&currentFiles) - 1) {
			++fileIndex;
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
			size_t len = strlen(currentPath);
			const char* sep = PATH_SEP;
			if (currentPath[len - 1] == *sep) {
				sep = "";
			}
			snprintf(currentPath, sizeof(currentPath), "%s%s%s", currentPath, sep, *FileListGetPointer(&currentFiles, fileIndex));
			if (!_refreshDirectory(currentPath, &currentFiles)) {
				strncpy(outPath, currentPath, outLen);
				return true;
			}
			fileIndex = 0;
		}
		if (newInput & (1 << GUI_INPUT_BACK)) {
			if (strncmp(currentPath, basePath, sizeof(currentPath)) == 0) {
				_cleanFiles(&currentFiles);
				FileListDeinit(&currentFiles);
				return false;
			}
			_upDirectory(currentPath);
			_refreshDirectory(currentPath, &currentFiles);
			fileIndex = 0;
		}

		params->drawStart();
		int y = GUIFontHeight(params->font);
		GUIFontPrintf(params->font, 0, y, GUI_TEXT_LEFT, 0xFFFFFFFF, "Current directory: %s", currentPath);
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
