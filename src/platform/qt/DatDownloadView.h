/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DAT_DOWNLOAD_VIEW
#define QGBA_DAT_DOWNLOAD_VIEW

#include <QNetworkReply>

#include "ui_DatDownloadView.h"

class QNetworkAccessManager;

namespace QGBA {

class DatDownloadView : public QDialog {
Q_OBJECT

public:
	DatDownloadView(QWidget* parent = nullptr);

public slots:
	void start();

private slots:
	void finished(QNetworkReply*);
	void downloadProgress(qint64, qint64);
	void errored(QNetworkReply::NetworkError);
	void buttonPressed(QAbstractButton* button);

private:
	Ui::DatDownloadView m_ui;
	QNetworkAccessManager* m_netman;
	QNetworkReply* m_reply;
};

}

#endif
