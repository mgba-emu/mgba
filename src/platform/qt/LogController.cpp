/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogController.h"

using namespace QGBA;

LogController LogController::s_global(mLOG_ALL);

LogController::LogController(int levels, QObject* parent)
	: QObject(parent)
	, m_logLevel(levels)
{
	if (this != &s_global) {
		connect(&s_global, &LogController::logPosted, this, &LogController::postLog);
		connect(this, &LogController::levelsSet, &s_global, &LogController::setLevels);
		connect(this, &LogController::levelsEnabled, &s_global, &LogController::enableLevels);
		connect(this, &LogController::levelsDisabled, &s_global, &LogController::disableLevels);
	}
}

LogController::Stream LogController::operator()(int category, int level) {
	return Stream(this, category, level);
}

void LogController::postLog(int level, int category, const QString& string) {
	if (!(m_logLevel & level)) {
		return;
	}
	emit logPosted(level, category, string);
}

void LogController::setLevels(int levels) {
	m_logLevel = levels;
	emit levelsSet(levels);
}

void LogController::enableLevels(int levels) {
	m_logLevel |= levels;
	emit levelsEnabled(levels);
}

void LogController::disableLevels(int levels) {
	m_logLevel &= ~levels;
	emit levelsDisabled(levels);
}

LogController* LogController::global() {
	return &s_global;
}

QString LogController::toString(int level) {
	switch (level) {
	case mLOG_DEBUG:
		return tr("DEBUG");
	case mLOG_STUB:
		return tr("STUB");
	case mLOG_INFO:
		return tr("INFO");
	case mLOG_WARN:
		return tr("WARN");
	case mLOG_ERROR:
		return tr("ERROR");
	case mLOG_FATAL:
		return tr("FATAL");
	case mLOG_GAME_ERROR:
		return tr("GAME ERROR");
	}
	return QString();
}

LogController::Stream::Stream(LogController* controller, int level, int category)
	: m_level(level)
	, m_category(category)
	, m_log(controller)
{
}

LogController::Stream::~Stream() {
	m_log->postLog(m_level, m_category, m_queue.join(" "));
}

LogController::Stream& LogController::Stream::operator<<(const QString& string) {
	m_queue.append(string);
	return *this;
}
