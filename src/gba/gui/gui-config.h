/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GUI_CONFIG_H
#define GUI_CONFIG_H

#include "util/common.h"

struct GBAGUIRunner;
struct GUIMenuItem;
void GBAGUIShowConfig(struct GBAGUIRunner* runner, struct GUIMenuItem* extra, size_t nExtra);

#endif
