/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LOG_VIEW
#define QGBA_LOG_VIEW

#include <QWidget>

#include "ui_LogView.h"

extern "C" {
#include "gba/supervisor/thread.h"
}

namespace QGBA {

class LogController;

class LogView : public QWidget {
Q_OBJECT

public:
	LogView(LogController* log, QWidget* parent = nullptr);

signals:
	void levelsEnabled(int levels);
	void levelsDisabled(int levels);

public slots:
	void postLog(int level, const QString& log);
	void setLevels(int levels);
	void clear();

private slots:
	void setMaxLines(int);

private:
	static const int DEFAULT_LINE_LIMIT = 1000;

	Ui::LogView m_ui;
	int m_lines;
	int m_lineLimit;

	void setLevel(int level, bool);

	void clearLine();
};

}

#endif
