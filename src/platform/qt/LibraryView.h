/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LIBRARY_VIEW
#define QGBA_LIBRARY_VIEW

#include "LibraryModel.h"

#include "ui_LibraryView.h"

struct VFile;

namespace QGBA {

class LibraryView : public QWidget {
Q_OBJECT

public:
	LibraryView(QWidget* parent = nullptr);

	VFile* selectedVFile() const;
	QPair<QString, QString> selectedPath() const;

signals:
	void doneLoading();
	void accepted();

public slots:
	void setDirectory(const QString&);
	void addDirectory(const QString&);

private slots:
	void resizeColumns();

private:
	Ui::LibraryView m_ui;

	LibraryModel m_model;
};

}

#endif
