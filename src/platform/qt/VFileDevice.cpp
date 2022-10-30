/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VFileDevice.h"

#include <QBuffer>

#include <mgba-util/vfs.h>

using namespace QGBA;

namespace QGBA {

class VFileAbstractWrapper : public VFile {
public:
	VFileAbstractWrapper(QIODevice*);

protected:
	QIODevice* m_iodev;

private:
	static bool close(struct VFile* vf);
	static off_t seek(struct VFile* vf, off_t offset, int whence);
	static ssize_t read(struct VFile* vf, void* buffer, size_t size);
	static ssize_t readline(struct VFile* vf, char* buffer, size_t size);
	static ssize_t write(struct VFile* vf, const void* buffer, size_t size);
	static void* map(struct VFile* vf, size_t size, int flags);
	static void unmap(struct VFile* vf, void* memory, size_t size);
	static void truncate(struct VFile* vf, size_t size);
	static ssize_t size(struct VFile* vf);
	static bool sync(struct VFile* vf, void* buffer, size_t size);

};

class VFileWrapper : public VFileAbstractWrapper {
public:
	VFileWrapper(QFileDevice*);

protected:
	QFileDevice* iodev() { return static_cast<QFileDevice*>(m_iodev); }

private:
	static bool close(struct VFile* vf);
	static void* map(struct VFile* vf, size_t size, int flags);
	static void unmap(struct VFile* vf, void* memory, size_t size);
	static void truncate(struct VFile* vf, size_t size);
	static bool sync(struct VFile* vf, void* buffer, size_t size);
};

class VFileBufferWrapper : public VFileAbstractWrapper {
public:
	VFileBufferWrapper(QBuffer*);

protected:
	QBuffer* iodev() { return static_cast<QBuffer*>(m_iodev); }

private:
	static bool close(struct VFile* vf);
	static void* map(struct VFile* vf, size_t size, int flags);
	static void unmap(struct VFile* vf, void* memory, size_t size);
};

}

VFileDevice::VFileDevice(VFile* vf, QObject* parent)
	: QIODevice(parent)
	, m_vf(vf)
{
	// TODO: Correct mode
	if (vf) {
		setOpenMode(QIODevice::ReadWrite);
	}
}

VFileDevice::VFileDevice(const QString& filename, QIODevice::OpenMode mode, QObject* parent)
	: QIODevice(parent)
{
	int posixMode = 0;
	if ((mode & QIODevice::ReadWrite) == QIODevice::ReadWrite) {
		posixMode = O_RDWR;
	} else if (mode & QIODevice::ReadOnly) {
		posixMode = O_RDONLY;
	} else if (mode & QIODevice::WriteOnly) {
		posixMode = O_WRONLY;
	}
	m_vf = open(filename, posixMode);
	if (m_vf) {
		setOpenMode(mode);
	}
}

VFileDevice::VFileDevice(const QByteArray& mem, QObject* parent)
	: QIODevice(parent)
	, m_vf(VFileMemChunk(mem.constData(), mem.size()))
{
	setOpenMode(QIODevice::ReadWrite);
}

VFileDevice::~VFileDevice() {
	close();
}

void VFileDevice::close() {
	if (!m_vf) {
		return;
	}
	QIODevice::close();
	m_vf->close(m_vf);
	m_vf = nullptr;
}

bool VFileDevice::resize(qint64 sz) {
	m_vf->truncate(m_vf, sz);
	return true;
}

bool VFileDevice::seek(qint64 pos) {
	QIODevice::seek(pos);
	return m_vf->seek(m_vf, pos, SEEK_SET) == pos;
}

VFileDevice& VFileDevice::operator=(VFile* vf) {
	close();
	m_vf = vf;
	setOpenMode(QIODevice::ReadWrite);
	return *this;
}

VFile* VFileDevice::take() {
	VFile* vf = m_vf;
	m_vf = nullptr;
	QIODevice::close();
	return vf;
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

VFile* VFileDevice::wrap(QIODevice* iodev, QIODevice::OpenMode mode) {
	if (!iodev->open(mode)) {
		return nullptr;
	}
	return new VFileAbstractWrapper(iodev);
}

VFile* VFileDevice::wrap(QFileDevice* iodev, QIODevice::OpenMode mode) {
	if (!iodev->open(mode)) {
		return nullptr;
	}
	return new VFileWrapper(iodev);
}

VFile* VFileDevice::wrap(QBuffer* iodev, QIODevice::OpenMode mode) {
	if (!iodev->open(mode)) {
		return nullptr;
	}
	return new VFileBufferWrapper(iodev);
}

VFile* VFileDevice::open(const QString& path, int mode) {
	return VFileOpen(path.toUtf8().constData(), mode);
}

VFile* VFileDevice::openMemory(quint64 size) {
	return VFileMemChunk(nullptr, size);
}

VDir* VFileDevice::openDir(const QString& path) {
	return VDirOpen(path.toUtf8().constData());
}

VDir* VFileDevice::openArchive(const QString& path) {
	return VDirOpenArchive(path.toUtf8().constData());
}

bool VFileDevice::copyFile(VFile* input, VFile* output) {
	uint8_t buffer[0x800];

	input->seek(input, 0, SEEK_SET);
	output->seek(output, 0, SEEK_SET);

	ssize_t size;
	while ((size = input->read(input, buffer, sizeof(buffer))) > 0) {
		output->write(output, buffer, size);
	}
	return size >= 0;
}

VFileAbstractWrapper::VFileAbstractWrapper(QIODevice* iodev)
	: m_iodev(iodev)
{
	VFile::close = &VFileAbstractWrapper::close;
	VFile::seek = &VFileAbstractWrapper::seek;
	VFile::read = &VFileAbstractWrapper::read;
	VFile::readline = &VFileAbstractWrapper::readline;
	VFile::write = &VFileAbstractWrapper::write;
	VFile::map = &VFileAbstractWrapper::map;
	VFile::unmap = &VFileAbstractWrapper::unmap;
	VFile::truncate = &VFileAbstractWrapper::truncate;
	VFile::size = &VFileAbstractWrapper::size;
	VFile::sync = &VFileAbstractWrapper::sync;
}

bool VFileAbstractWrapper::close(VFile* vf) {
	QIODevice* iodev = static_cast<VFileAbstractWrapper*>(vf)->m_iodev;
	iodev->close();
	delete static_cast<VFileAbstractWrapper*>(vf);
	return true;
}

off_t VFileAbstractWrapper::seek(VFile* vf, off_t offset, int whence) {
	QIODevice* iodev = static_cast<VFileAbstractWrapper*>(vf)->m_iodev;
	switch (whence) {
	case SEEK_SET:
		if (!iodev->seek(offset)) {
			return -1;
		}
		break;
	case SEEK_CUR:
		if (!iodev->seek(iodev->pos() + offset)) {
			return -1;
		}
		break;
	case SEEK_END:
		if (!iodev->seek(iodev->size() + offset)) {
			return -1;
		}
		break;
	}
	return iodev->pos();
}

ssize_t VFileAbstractWrapper::read(VFile* vf, void* buffer, size_t size) {
	QIODevice* iodev = static_cast<VFileAbstractWrapper*>(vf)->m_iodev;
	return iodev->read(static_cast<char*>(buffer), size);
}

ssize_t VFileAbstractWrapper::readline(VFile* vf, char* buffer, size_t size) {
	QIODevice* iodev = static_cast<VFileAbstractWrapper*>(vf)->m_iodev;
	return iodev->readLine(static_cast<char*>(buffer), size);
}

ssize_t VFileAbstractWrapper::write(VFile* vf, const void* buffer, size_t size) {
	QIODevice* iodev = static_cast<VFileAbstractWrapper*>(vf)->m_iodev;
	return iodev->write(static_cast<const char*>(buffer), size);
}

void* VFileAbstractWrapper::map(VFile*, size_t, int) {
	// Doesn't work on QIODevice base class
	return nullptr;
}

void VFileAbstractWrapper::unmap(VFile*, void*, size_t) {
	// Doesn't work on QIODevice base class
}

void VFileAbstractWrapper::truncate(VFile*, size_t) {
	// Doesn't work on QIODevice base class
}

ssize_t VFileAbstractWrapper::size(VFile* vf) {
	QIODevice* iodev = static_cast<VFileAbstractWrapper*>(vf)->m_iodev;
	return iodev->size();
}

bool VFileAbstractWrapper::sync(VFile*, void*, size_t) {
	// Doesn't work on QIODevice base class
	return false;
}

VFileWrapper::VFileWrapper(QFileDevice* iodev)
	: VFileAbstractWrapper(iodev)
{
	VFile::close = &VFileWrapper::close;
	VFile::map = &VFileWrapper::map;
	VFile::unmap = &VFileWrapper::unmap;
	VFile::truncate = &VFileWrapper::truncate;
	VFile::sync = &VFileWrapper::sync;
}

bool VFileWrapper::close(VFile* vf) {
	QIODevice* iodev = static_cast<VFileWrapper*>(vf)->m_iodev;
	iodev->close();
	delete static_cast<VFileWrapper*>(vf);
	return true;
}

void* VFileWrapper::map(VFile* vf, size_t size, int mode) {
	QFileDevice* iodev = static_cast<VFileWrapper*>(vf)->iodev();
	return iodev->map(0, size, mode == MAP_READ ? QFileDevice::MapPrivateOption : QFileDevice::NoOptions);
}

void VFileWrapper::unmap(VFile* vf, void* buffer, size_t) {
	QFileDevice* iodev = static_cast<VFileWrapper*>(vf)->iodev();
	iodev->unmap(static_cast<uchar*>(buffer));
}

void VFileWrapper::truncate(VFile* vf, size_t size) {
	QFileDevice* iodev = static_cast<VFileWrapper*>(vf)->iodev();
	iodev->resize(size);
}

bool VFileWrapper::sync(VFile* vf, void*, size_t) {
	QFileDevice* iodev = static_cast<VFileWrapper*>(vf)->iodev();
	return iodev->flush();
}

VFileBufferWrapper::VFileBufferWrapper(QBuffer* iodev)
	: VFileAbstractWrapper(iodev)
{
	VFile::close = &VFileBufferWrapper::close;
	VFile::map = &VFileBufferWrapper::map;
}

bool VFileBufferWrapper::close(VFile* vf) {
	QIODevice* iodev = static_cast<VFileBufferWrapper*>(vf)->m_iodev;
	iodev->close();
	delete static_cast<VFileBufferWrapper*>(vf);
	return true;
}

void* VFileBufferWrapper::map(VFile* vf, size_t, int) {
	QBuffer* iodev = static_cast<VFileBufferWrapper*>(vf)->iodev();
	QByteArray& buffer = iodev->buffer();
	return static_cast<void*>(buffer.data());
}
