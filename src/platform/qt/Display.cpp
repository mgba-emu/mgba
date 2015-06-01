/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Display.h"

extern "C" {
#include "gba/video.h"
}

using namespace QGBA;

Display::Display(QWidget* parent)
	: QWidget(parent)
{
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	setMinimumSize(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
}
