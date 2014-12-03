/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VFileDevice.h"

extern "C" {
#include "util/vfs.h"
}

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
	// TODO: Add size method to VFile so this can be actually const
	ssize_t pos = m_vf->seek(m_vf, 0, SEEK_CUR);
	qint64 size = m_vf->seek(m_vf, 0, SEEK_END);
	m_vf->seek(m_vf, pos, SEEK_SET);
	return size;
}
