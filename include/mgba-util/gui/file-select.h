/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_FILE_CHOOSER_H
#define GUI_FILE_CHOOSER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/gui.h>

struct VFile;

bool GUISelectFile(struct GUIParams*, char* outPath, size_t outLen, bool (*filterName)(const char* name), bool (*filterContents)(struct VFile*), const char* preselect);

CXX_GUARD_END

#endif
