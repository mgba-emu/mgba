/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DatDownloadView.h"

#include "GBAApp.h"

#include <QFile>
#include <QNetworkAccessManager>

extern "C" {
#include "gba/context/config.h"
#include "util/version.h"
}

using namespace QGBA;

DatDownloadView::DatDownloadView(QWidget* parent)
	: QDialog(parent)
	, m_reply(nullptr)
{
	m_ui.setupUi(this);

	m_netman = new QNetworkAccessManager(this);
	connect(m_netman, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));
	connect(m_ui.dialogButtonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonPressed(QAbstractButton*)));
}

void DatDownloadView::start() {
	if (m_reply) {
		return;
	}
	QNetworkRequest request(QUrl("https://raw.githubusercontent.com/mgba-emu/mgba/master/res/nointro.dat"));
	request.setHeader(QNetworkRequest::UserAgentHeader, QString("%1 %2").arg(projectName).arg(projectVersion));
	m_reply = m_netman->get(request);
	connect(m_reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
	connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errored(QNetworkReply::NetworkError)));
}

void DatDownloadView::finished(QNetworkReply* reply) {
	if (!m_reply) {
		return;
	}
	char path[PATH_MAX];
	GBAConfigDirectory(path, sizeof(path));
	QFile outfile(QString::fromUtf8(path) + "/nointro.dat");
	outfile.open(QIODevice::WriteOnly);
	outfile.write(m_reply->readAll());
	GBAApp::app()->reloadGameDB();

	m_reply->deleteLater();
	m_reply = nullptr;
	setAttribute(Qt::WA_DeleteOnClose);
	close();
}

void DatDownloadView::downloadProgress(qint64 read, qint64 size) {
	if (size < 0) {
		return;
	}
	m_ui.progressBar->setMaximum(size);
	m_ui.progressBar->setValue(read);
}

void DatDownloadView::errored(QNetworkReply::NetworkError) {
	m_ui.status->setText(tr("An error occurred"));
	m_reply->deleteLater();
	m_reply = nullptr;
}

void DatDownloadView::buttonPressed(QAbstractButton* button) {
	switch (m_ui.dialogButtonBox->standardButton(button)) {
	case QDialogButtonBox::Cancel:
		if (m_reply) {
			m_reply->abort();
		}
	 	break;
	default:
		break;
	}
}
