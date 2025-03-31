/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <mgba/core/core.h>

struct mLibraryEntry;

namespace QGBA {

struct LibraryEntry {
	LibraryEntry() = default;
	LibraryEntry(const LibraryEntry&) = default;
	LibraryEntry(LibraryEntry&&) = default;
	LibraryEntry(const mLibraryEntry* entry);

	bool isNull() const;

	QString displayTitle(bool showFilename = false) const;
	QString displayPlatform() const;

	QString base;
	QString filename;
	QString fullpath;
	QString title;
	QByteArray internalTitle;
	QByteArray internalCode;
	mPlatform platform;
	int platformModels;
	size_t filesize;
	uint32_t crc32;
	QByteArray sha1;

	LibraryEntry& operator=(const LibraryEntry&) = default;
	LibraryEntry& operator=(LibraryEntry&&) = default;
	bool operator==(const LibraryEntry& other) const;

	uint64_t checkHash() const;
	static uint64_t checkHash(const mLibraryEntry* entry);
};

};
