/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_FILE_CHOOSER_H
#define GUI_FILE_CHOOSER_H

#include "util/gui.h"

bool selectFile(const struct GUIParams*, const char* basePath, char* outPath, size_t outLen, const char* suffix);

#endif
