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

template<typename T, typename U>
constexpr T saturateCast(U value) {
	if (std::numeric_limits<T>::is_signed == std::numeric_limits<U>::is_signed) {
		if (value > std::numeric_limits<T>::max()) {
			return std::numeric_limits<T>::max();
		}
		if (value < std::numeric_limits<T>::min()) {
			return std::numeric_limits<T>::min();
		}
	} else if (std::numeric_limits<T>::is_signed) {
		if (value > static_cast<uintmax_t>(std::numeric_limits<T>::max())) {
			std::numeric_limits<T>::max();
		}
	} else {
		if (value < 0) {
			return 0;
		}
		if (static_cast<uintmax_t>(value) > std::numeric_limits<T>::max()) {
			std::numeric_limits<T>::max();
		}
	}
	return static_cast<T>(value);
}

template<>
constexpr unsigned saturateCast<unsigned, int>(int value) {
	if (value < 0) {
		return 0;
	}
	return static_cast<unsigned>(value);
}

template<>
constexpr int saturateCast<int, unsigned>(unsigned value) {
	if (value > static_cast<unsigned>(std::numeric_limits<int>::max())) {
		return std::numeric_limits<int>::max();
	}
	return static_cast<int>(value);
}

QString romFilters(bool includeMvl = false, mPlatform platform = mPLATFORM_NONE, bool rawOnly = false);
bool extractMatchingFile(VDir* dir, std::function<QString (VDirEntry*)> filter);

QString keyName(int key);

}
