/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct Rectangle {
    int x;
    int y;
    unsigned width;
    unsigned height;
};

CXX_GUARD_END

#endif
