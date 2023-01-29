/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AboutScreen.h"

#include <mgba/core/version.h>

#include <QFile>
#include <QPixmap>

using namespace QGBA;

AboutScreen::AboutScreen(QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

	QPixmap logo(":/res/mgba-1024.png");
	logo = logo.scaled(m_ui.logo->minimumSize() * devicePixelRatio(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	logo.setDevicePixelRatio(devicePixelRatio());
	m_ui.logo->setPixmap(logo);

	QLatin1String tree(gitBranch);
	if (tree == QLatin1String("(unknown)")) {
		tree = QLatin1String(projectVersion);
	}

	QFile patronFile(":/res/patrons.txt");
	QStringList patronList;
	patronFile.open(QIODevice::ReadOnly | QIODevice::Text);
	while (true) {
		QByteArray line = patronFile.readLine();
		if (line.isEmpty()) {
			break;
		}
		patronList.append(QString::fromUtf8(line).trimmed());
	}

	m_ui.projectName->setText(QLatin1String(projectName));
	m_ui.projectVersion->setText(QLatin1String(projectVersion));

	{
		QString gitInfo = m_ui.gitInfo->text();
		gitInfo.replace("{gitBranch}", QLatin1String(gitBranch));
		gitInfo.replace("{gitCommit}", QLatin1String(gitCommit));
		m_ui.gitInfo->setText(gitInfo);
	}

	{
		QString description = m_ui.description->text();
		description.replace("{projectName}", QLatin1String(projectName));
		m_ui.description->setText(description);
	}

	{
		QString extraLinks = m_ui.extraLinks->text();
		extraLinks.replace("{gitBranch}", tree);
		m_ui.extraLinks->setText(extraLinks);
	}

	{
		QString patronsHeader = m_ui.patronsHeader->text();
		patronsHeader.replace("{projectName}", QLatin1String(projectName));
		m_ui.patronsHeader->setText(patronsHeader);
	}

	{
		QString patrons = m_ui.patrons->text();
		patrons.replace("{patrons}", patronList.join(" • "));
		m_ui.patrons->setText(patrons);
	}

	{
		QString copyright = m_ui.copyright->text();
		copyright.replace("{year}", QLatin1String("2023"));
		m_ui.copyright->setText(copyright);
	}
}
