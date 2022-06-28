/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryEntry.h"

#include <mgba/core/library.h>

using namespace QGBA;

static inline uint64_t checkHash(size_t filesize, uint32_t crc32) {
	return (uint64_t(filesize) << 32) ^ ((crc32 + 1ULL) * (uint32_t(filesize) + 1ULL));
}

LibraryEntry::LibraryEntry(const mLibraryEntry* entry)
	: base(entry->base)
	, filename(entry->filename)
	, fullpath(QString("%1/%2").arg(entry->base, entry->filename))
	, title(entry->title)
	, internalTitle(entry->internalTitle)
	, internalCode(entry->internalCode)
	, platform(entry->platform)
	, filesize(entry->filesize)
	, crc32(entry->crc32)
{
}

bool LibraryEntry::isNull() const {
	return fullpath.isNull();
}

QString LibraryEntry::displayTitle(bool showFilename) const {
	if (showFilename || title.isNull()) {
		return filename;
	}
	return title;
}

bool LibraryEntry::operator==(const LibraryEntry& other) const {
	return other.fullpath == fullpath;
}

uint64_t LibraryEntry::checkHash() const {
	return ::checkHash(filesize, crc32);
}

uint64_t LibraryEntry::checkHash(const mLibraryEntry* entry) {
	return ::checkHash(entry->filesize, entry->crc32);
}
