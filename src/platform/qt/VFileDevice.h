/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QFileDevice>

struct VDir;
struct VFile;

class QBuffer;

namespace QGBA {

class VFileDevice : public QIODevice {
Q_OBJECT

public:
	VFileDevice(VFile* vf = nullptr, QObject* parent = nullptr);
	VFileDevice(const QByteArray& mem, QObject* parent = nullptr);
	VFileDevice(const QString&, QIODevice::OpenMode, QObject* parent = nullptr);
	virtual ~VFileDevice();

	virtual void close() override;
	virtual bool seek(qint64 pos) override;
	virtual qint64 size() const override;

	bool resize(qint64 sz);

	VFileDevice& operator=(VFile*);
	operator VFile*() { return m_vf; }
	VFile* take();

	static VFile* wrap(QIODevice*, QIODevice::OpenMode);
	static VFile* wrap(QFileDevice*, QIODevice::OpenMode);
	static VFile* wrap(QBuffer*, QIODevice::OpenMode);

	static VFile* open(const QString& path, int mode);
	static VFile* openMemory(quint64 size = 0);
	static VDir* openDir(const QString& path);
	static VDir* openArchive(const QString& path);

	static bool copyFile(VFile* input, VFile* output);

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;

private:
	VFile* m_vf;
};

}
