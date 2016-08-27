/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_ARCHIVE_INSPECTOR
#define QGBA_ARCHIVE_INSPECTOR

#include "LibraryModel.h"

#include "ui_ArchiveInspector.h"

struct VDir;
struct VFile;

namespace QGBA {

class ArchiveInspector : public QDialog {
Q_OBJECT

public:
	ArchiveInspector(const QString& filename, QWidget* parent = nullptr);
	virtual ~ArchiveInspector();

	VFile* selectedVFile() const;

private:
	Ui::ArchiveInspector m_ui;

	LibraryModel m_model;
	VDir* m_dir;
};

}

#endif
