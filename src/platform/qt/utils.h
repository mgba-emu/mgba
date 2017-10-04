/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <mgba/core/core.h>

#include <QString>

namespace QGBA {

QString niceSizeFormat(size_t filesize);
QString nicePlatformFormat(mPlatform platform);

}
