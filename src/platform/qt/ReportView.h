/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>
#include <QHash>

#include <array>
#include <memory>

#include "ConfigController.h"

#include "ui_ReportView.h"

namespace QGBA {

class ConfigController;
class CoreController;

class ReportView : public QDialog {
Q_OBJECT

public:
	ReportView(QWidget* parent = nullptr);

public slots:
	void generateReport();
	void save();

private slots:
	void setShownReport(const QString&);
	void rebuildModel();
	void openBugReportPage();

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	void addCpuInfo(QStringList&);
	void addGLInfo(QStringList&);
	void addGamepadInfo(QStringList&);
	void addROMInfo(QStringList&, CoreController*);
	void addScreenInfo(QStringList&, const QScreen*);

	void addReport(const QString& filename, const QString& report);
	void addBinary(const QString& filename, const QByteArray& report);
	QString redact(const QString& text);

#if (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))) || (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64)))
	static bool cpuid(unsigned id, unsigned* regs);
	static bool cpuid(unsigned id, unsigned sub, unsigned* regs);

	static unsigned s_cpuidMax;
	static unsigned s_cpuidExtMax;
#endif

	ConfigController* m_config;

	QStringList m_displayOrder;
	QHash<QString, QString> m_reports;
	QHash<QString, QByteArray> m_binaries;

	Ui::ReportView m_ui;
};

}
