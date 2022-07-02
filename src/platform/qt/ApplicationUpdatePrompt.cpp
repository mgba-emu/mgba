/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ApplicationUpdatePrompt.h"

#include <QCryptographicHash>
#include <QPushButton>

#include "ApplicationUpdater.h"
#include "GBAApp.h"
#include "utils.h"

#include <mgba/core/version.h>

using namespace QGBA;

ApplicationUpdatePrompt::ApplicationUpdatePrompt(QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

	ApplicationUpdater* updater = GBAApp::app()->updater();
	ApplicationUpdater::UpdateInfo info = updater->updateInfo();
	QString updateText(tr("An update to %1 is available.\n").arg(QLatin1String(projectName)));
	bool available;
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
	available = true;
#elif defined(Q_OS_LINUX)
	QString path = QCoreApplication::applicationDirPath();
	QFileInfo finfo(path + "/../../AppRun");
	available = finfo.exists() && finfo.isExecutable();
#else
	available = false;
#endif
	if (available) {
		updateText += tr("\nDo you want to download and install it now? You will need to restart the emulator when the download is complete.");
		m_okDownload = connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &ApplicationUpdatePrompt::startUpdate);
	} else {
		updateText += tr("\nAuto-update is not available on this platform. If you wish to update you will need to do it manually.");
		connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &QWidget::close);
	}
	m_ui.text->setText(updateText);
	m_ui.details->setText(tr("Current version: %1\nNew version: %2\nDownload size: %3")
		.arg(QLatin1String(projectVersion))
		.arg(info)
		.arg(niceSizeFormat(info.size)));
	m_ui.progressBar->setVisible(false);

	connect(updater, &AbstractUpdater::updateProgress, this, [this](float progress) {
		m_ui.progressBar->setValue(progress * 100);
	});
	connect(updater, &AbstractUpdater::updateDone, this, &ApplicationUpdatePrompt::promptRestart);
}

void ApplicationUpdatePrompt::startUpdate() {
	ApplicationUpdater* updater = GBAApp::app()->updater();
	updater->downloadUpdate();

	m_ui.buttonBox->disconnect(m_okDownload);
	m_ui.progressBar->show();
	m_ui.text->setText(tr("Downloading update..."));
	m_ui.details->hide();
	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
}

void ApplicationUpdatePrompt::promptRestart() {
	ApplicationUpdater* updater = GBAApp::app()->updater();
	QString filename = updater->destination();

	QByteArray expectedHash = updater->updateInfo().sha256;
	QCryptographicHash sha256(QCryptographicHash::Sha256);
	QFile update(filename);
	update.open(QIODevice::ReadOnly);
	if (!sha256.addData(&update) || sha256.result() != expectedHash) {
		update.close();
		update.remove();
		m_ui.text->setText(tr("Downloading failed. Please update manually.")
			.arg(QLatin1String(projectName)));
	} else {
		m_ui.text->setText(tr("Downloading done. Press OK to restart %1 and install the update.")
			.arg(QLatin1String(projectName)));
	}
	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
