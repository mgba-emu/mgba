/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VFileDevice.h"

#include <mgba-util/vfs.h>

using namespace QGBA;

VFileDevice::VFileDevice(VFile* vf, QObject* parent)
	: QIODevice(parent)
	, m_vf(vf)
{
	// Nothing to do
}

qint64 VFileDevice::readData(char* data, qint64 maxSize) {
	return m_vf->read(m_vf, data, maxSize);
}

qint64 VFileDevice::writeData(const char* data, qint64 maxSize) {
	return m_vf->write(m_vf, data, maxSize);
}

qint64 VFileDevice::size() const {
	return m_vf->size(m_vf);
}

VFile* VFileDevice::open(const QString& path, int mode) {
	return VFileOpen(path.toUtf8().constData(), mode);
}

VDir* VFileDevice::openDir(const QString& path) {
	return VDirOpen(path.toUtf8().constData());
}
VDir* VFileDevice::openArchive(const QString& path) {
	return VDirOpenArchive(path.toUtf8().constData());
}
