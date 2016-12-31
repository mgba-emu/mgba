/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_VFILE_DEVICE
#define QGBA_VFILE_DEVICE

#include <QFileDevice>

struct VDir;
struct VFile;

namespace QGBA {

class VFileDevice : public QIODevice {
Q_OBJECT

public:
	VFileDevice(VFile* vf, QObject* parent = nullptr);

	static VFile* open(const QString& path, int mode);
	static VDir* openDir(const QString& path);
	static VDir* openArchive(const QString& path);

protected:
	virtual qint64 readData(char* data, qint64 maxSize) override;
	virtual qint64 writeData(const char* data, qint64 maxSize) override;
	virtual qint64 size() const override;

private:
	VFile* m_vf;
};

}

#endif
