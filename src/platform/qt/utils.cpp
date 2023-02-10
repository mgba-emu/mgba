/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "utils.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QObject>

namespace QGBA {

QString niceSizeFormat(size_t filesize) {
	double size = filesize;
	QString unit = QObject::tr("%1 byte");
	if (size >= 1024.0) {
		size /= 1024.0;
		unit = QObject::tr("%1 kiB");
	}
	if (size >= 1024.0) {
		size /= 1024.0;
		unit = QObject::tr("%1 MiB");
	}
	return unit.arg(size, 0, 'f', int(size * 10) % 10 ? 1 : 0);
}

QString nicePlatformFormat(mPlatform platform) {
	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		return QObject::tr("GBA");
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		return QObject::tr("GB");
#endif
	default:
		return QObject::tr("?");
	}
}

bool convertAddress(const QHostAddress* input, Address* output) {
	if (input->isNull()) {
		return false;
	}
	Q_IPV6ADDR ipv6;
	switch (input->protocol()) {
	case QAbstractSocket::IPv4Protocol:
		output->version = IPV4;
		output->ipv4 = input->toIPv4Address();
		break;
	case QAbstractSocket::IPv6Protocol:
		output->version = IPV6;
		ipv6 = input->toIPv6Address();
		memcpy(output->ipv6, &ipv6, 16);
		break;
	default:
		return false;
	}
	return true;
}

QString keyName(int key) {
	switch (key) {
#ifndef Q_OS_MAC
	case Qt::Key_Shift:
		return QCoreApplication::translate("QShortcut", "Shift");
	case Qt::Key_Control:
		return QCoreApplication::translate("QShortcut", "Control");
	case Qt::Key_Alt:
		return QCoreApplication::translate("QShortcut", "Alt");
	case Qt::Key_Meta:
		return QCoreApplication::translate("QShortcut", "Meta");
#endif
	case Qt::Key_Super_L:
		return QObject::tr("Super (L)");
	case Qt::Key_Super_R:
		return QObject::tr("Super (R)");
	case Qt::Key_Menu:
		return QObject::tr("Menu");
	default:
		return QKeySequence(key).toString(QKeySequence::NativeText);
	}
}

}
