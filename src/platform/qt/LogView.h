/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LOG_VIEW
#define QGBA_LOG_VIEW

#include <QWidget>

#include "ui_LogView.h"

extern "C" {
#include "gba-thread.h"
}

namespace QGBA {

class LogView : public QWidget {
Q_OBJECT

public:
	LogView(QWidget* parent = nullptr);

signals:
	void levelsSet(int levels);
	void levelsEnabled(int levels);
	void levelsDisabled(int levels);

public slots:
	void postLog(int level, const QString& log);
	void setLevels(int levels);
	void clear();

	void setLevelDebug(bool);
	void setLevelStub(bool);
	void setLevelInfo(bool);
	void setLevelWarn(bool);
	void setLevelError(bool);
	void setLevelFatal(bool);
	void setLevelGameError(bool);
	void setLevelSWI(bool);

	void setMaxLines(int);

private:
	static const int DEFAULT_LINE_LIMIT = 1000;

	Ui::LogView m_ui;
	int m_logLevel;
	int m_lines;
	int m_lineLimit;

	static QString toString(int level);
	void setLevel(int level);
	void clearLevel(int level);

	void clearLine();
};

}

#endif
