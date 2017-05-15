/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "utils.h"

#include <QObject>

namespace QGBA {

QString niceSizeFormat(size_t filesize) {
	double size = filesize;
	QString unit = "B";
	if (size >= 1024.0) {
		size /= 1024.0;
		unit = "kiB";
	}
	if (size >= 1024.0) {
		size /= 1024.0;
		unit = "MiB";
	}
	return QString("%0 %1").arg(size, 0, 'f', 1).arg(unit);
}
QString nicePlatformFormat(mPlatform platform) {
	switch (platform) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		return QObject::tr("GBA");
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB:
		return QObject::tr("GB");
#endif
	default:
		return QObject::tr("?");
	}
}

}
