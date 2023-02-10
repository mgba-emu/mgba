/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <mgba/core/core.h>
#include <mgba-util/socket.h>
#include <mgba-util/vfs.h>

#include <QHostAddress>
#include <QRect>
#include <QSize>
#include <QString>

#include <algorithm>
#include <functional>

struct VDir;
struct VDirEntry;

namespace QGBA {

enum class Endian {
	NONE    = 0b00,
	BIG     = 0b01,
	LITTLE  = 0b10,
	UNKNOWN = 0b11
};

QString niceSizeFormat(size_t filesize);
QString nicePlatformFormat(mPlatform platform);

bool convertAddress(const QHostAddress* input, Address* output);

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

#if __cplusplus >= 201703L
using std::clamp;
#else
template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
	return std::max(lo, std::min(hi, v));
}
#endif

QString romFilters(bool includeMvl = false);
bool extractMatchingFile(VDir* dir, std::function<QString (VDirEntry*)> filter);

QString keyName(int key);

}
