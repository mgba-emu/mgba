/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <mgba/core/core.h>

#include <QRect>
#include <QSize>
#include <QString>

namespace QGBA {

QString niceSizeFormat(size_t filesize);
QString nicePlatformFormat(mPlatform platform);

inline void lockAspectRatio(const QSize& ref, QSize& size) {
	if (size.width() * ref.height() > size.height() * ref.width()) {
		size.setWidth(size.height() * ref.width() / ref.height());
	} else if (size.width() * ref.height() < size.height() * ref.width()) {
		size.setHeight(size.width() * ref.height() / ref.width());
	}
}

inline void lockIntegerScaling(const QSize& ref, QSize& size) {
	if (size.width() >= ref.width()) {
		size.setWidth(size.width() - size.width() % ref.width());
	}
	if (size.height() >= ref.height()) {
		size.setHeight(size.height() - size.height() % ref.height());
	}
}

inline QRect clampSize(const QSize& ref, const QSize& size, bool aspectRatio, bool integerScaling) {
	QSize ds = size;
	if (aspectRatio) {
		lockAspectRatio(ref, ds);
	}
	if (integerScaling) {
		QGBA::lockIntegerScaling(ref, ds);
	}
	QPoint origin = QPoint((size.width() - ds.width()) / 2, (size.height() - ds.height()) / 2);
	return QRect(origin, ds);
}

}
