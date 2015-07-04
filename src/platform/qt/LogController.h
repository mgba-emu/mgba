/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LOG_CONTROLLER
#define QGBA_LOG_CONTROLLER

#include <QObject>
#include <QStringList>

extern "C" {
#include "gba/gba.h"
}

namespace QGBA {

class LogController : public QObject {
Q_OBJECT

private:
	class Stream {
	public:
		Stream(LogController* controller, int level);
		~Stream();

		Stream& operator<<(const QString&);

	private:
		int m_level;
		LogController* m_log;

		QStringList m_queue;
	};

public:
	LogController(int levels, QObject* parent = nullptr);

	int levels() const { return m_logLevel; }

	Stream operator()(int level);

	static QString toString(int level);

signals:
	void logPosted(int level, const QString& log);
	void levelsSet(int levels);
	void levelsEnabled(int levels);
	void levelsDisabled(int levels);

public slots:
	void postLog(int level, const QString& string);
	void setLevels(int levels);
	void enableLevels(int levels);
	void disableLevels(int levels);

private:
	int m_logLevel;
};

}

#endif
