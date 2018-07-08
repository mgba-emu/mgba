/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "GBAApp.h"

#include <mgba/core/log.h>

#include <QObject>
#include <QStringList>

namespace QGBA {

class LogController : public QObject {
Q_OBJECT

private:
	class Stream {
	public:
		Stream(LogController* controller, int level, int category);
		~Stream();

		Stream& operator<<(const QString&);

	private:
		int m_level;
		int m_category;
		LogController* m_log;

		QStringList m_queue;
	};

public:
	LogController(int levels, QObject* parent = nullptr);
	~LogController();

	int levels() const { return m_filter.defaultLevels; }
	mLogFilter* filter() { return &m_filter; }

	Stream operator()(int category, int level);

	static LogController* global();
	static QString toString(int level);

signals:
	void logPosted(int level, int category, const QString& log);
	void levelsSet(int levels);
	void levelsEnabled(int levels);
	void levelsDisabled(int levels);

public slots:
	void postLog(int level, int category, const QString& string);
	void setLevels(int levels);
	void enableLevels(int levels);
	void disableLevels(int levels);

private:
	mLogFilter m_filter;

	static LogController s_global;
};

#define LOG(C, L) (*LogController::global())(mLOG_ ## L, _mLOG_CAT_ ## C ())

}
