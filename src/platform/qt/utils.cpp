/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "utils.h"

#include <mgba/core/library.h>
#ifdef M_CORE_GB
#include <mgba/gb/interface.h>
#endif

#include <QCoreApplication>
#include <QHostAddress>
#include <QKeySequence>
#include <QObject>

#include "VFileDevice.h"

#include <mgba/core/version.h>

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

QString nicePlatformFormat(mPlatform platform, int validModels) {
	switch (platform) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		return "GBA";
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		if (validModels != M_LIBRARY_MODEL_UNKNOWN) {
			if (validModels & GB_MODEL_CGB) {
				return "GBC";
			} else if (validModels & GB_MODEL_SGB) {
				return "SGB";
			}
		}
		return "GB";
#endif
	default:
		return "?";
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

QString romFilters(bool includeMvl, mPlatform platform, bool rawOnly) {
	QStringList filters;
	QStringList formats;

#ifdef M_CORE_GBA
	QStringList gbaFormats{
		"*.gba",
		"*.agb",
		"*.mb",
		"*.rom",
		"*.bin"};
	if (!rawOnly) {
		gbaFormats += QStringList{
#if defined(USE_LIBZIP) || defined(USE_MINIZIP)
			"*.zip",
#endif
#ifdef USE_LZMA
			"*.7z",
#endif
#ifdef USE_ELF
			"*.elf",
#endif
		};
	}
	if (platform == mPLATFORM_NONE || platform == mPLATFORM_GBA) {
		formats.append(gbaFormats);
	}
	if (platform == mPLATFORM_NONE) {
		filters.append(QCoreApplication::translate("QGBA", "Game Boy Advance ROMs (%1)", nullptr).arg(gbaFormats.join(QChar(' '))));
	}
#endif

#ifdef M_CORE_GB
	QStringList gbFormats{
		"*.gb",
		"*.gbc",
		"*.sgb",
		"*.rom",
		"*.bin"};

	if (!rawOnly) {
		gbFormats += QStringList{
#if defined(USE_LIBZIP) || defined(USE_MINIZIP)
			"*.zip",
#endif
#ifdef USE_LZMA
			"*.7z",
#endif
		};
	}
	if (platform == mPLATFORM_NONE || platform == mPLATFORM_GBA) {
		formats.append(gbFormats);
	}
	if (platform == mPLATFORM_NONE) {
		filters.append(QCoreApplication::translate("QGBA", "Game Boy ROMs (%1)", nullptr).arg(gbFormats.join(QChar(' '))));
	}
#endif

	formats.removeDuplicates();
	filters.prepend(QCoreApplication::translate("QGBA", "All ROMs (%1)", nullptr).arg(formats.join(QChar(' '))));
	if (includeMvl) {
		filters.append(QCoreApplication::translate("QGBA", "%1 Video Logs (*.mvl)", nullptr).arg(projectName));
	}
	return filters.join(";;");
}

bool extractMatchingFile(VDir* dir, std::function<QString (VDirEntry*)> filter) {
	for (VDirEntry* entry = dir->listNext(dir); entry; entry = dir->listNext(dir)) {
		QString target = filter(entry);
		if (target.isNull()) {
			continue;
		}
		VFile* outfile = VFileOpen(target.toUtf8().constData(), O_WRONLY | O_TRUNC | O_CREAT);
		if (!outfile) {
			return false;
		}
		VFile* infile = dir->openFile(dir, entry->name(entry), O_RDONLY);
		if (!infile) {
			outfile->close(outfile);
			return false;
		}
		VFileDevice::copyFile(infile, outfile);
		infile->close(infile);
		outfile->close(outfile);
		return true;
	}
	return false;
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

void SpanSet::add(int pos) {
	for (Span& span : spans) {
		if (pos == span.left - 1) {
			span.left = pos;
			return;
		} else if (pos == span.right + 1) {
			span.right = pos;
			return;
		}
	}
	spans << Span{ pos, pos };
}

void SpanSet::merge() {
	int numSpans = spans.size();
	if (!numSpans) {
		return;
	}
	sort();
	QVector<Span> merged({ spans[0] });
	int lastRight = merged[0].right;
	for (int i = 1; i < numSpans; i++) {
		int right = spans[i].right;
		if (spans[i].left - 1 <= lastRight) {
			merged.back().right = right;
		} else {
			merged << spans[i];
		}
		lastRight = right;
	}
	spans = merged;
}

void SpanSet::sort(bool reverse) {
	if (reverse) {
		std::sort(spans.begin(), spans.end(), std::greater<Span>());
	} else {
		std::sort(spans.begin(), spans.end());
	}
}

}
