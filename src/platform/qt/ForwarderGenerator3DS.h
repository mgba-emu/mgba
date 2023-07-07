/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "ForwarderGenerator.h"

#include <QProcess>

#include <memory>

namespace QGBA {

class ForwarderGenerator3DS final : public ForwarderGenerator {
Q_OBJECT

public:
	ForwarderGenerator3DS();

	QList<QPair<QString, QSize>> imageTypes() const override;
	System system() const override { return System::N3DS; }
	QString extension() const override { return QLatin1String("cia"); }

	virtual QStringList externalTools() const { return {"bannertool", "3dstool", "ctrtool", "makerom"}; }

	void rebuild(const QString& source, const QString& target) override;

private slots:
	void extractCia();
	void extractCxi();
	void extractExefs();
	void processCxi();
	void prepareRomfs();
	void buildRomfs();
	void buildSmdh();
	void buildBanner();
	void buildExefs();
	void buildCxi();
	void buildCia();

	void cleanup();

private:
	void init3dstoolArgs(QStringList& args, const QString& file, const QString& createType = {});

	std::unique_ptr<QProcess> m_currentProc;
	QString m_cia;
	QString m_cxi;
	QString m_target;
};

}
