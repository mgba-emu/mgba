/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogController.h"

using namespace QGBA;

LogController LogController::s_global(GBA_LOG_ALL);

LogController::LogController(int levels, QObject* parent)
	: QObject(parent)
	, m_logLevel(levels)
{
	if (this != &s_global) {
		connect(&s_global, SIGNAL(logPosted(int, const QString&)), this, SLOT(postLog(int, const QString&)));
		connect(this, SIGNAL(levelsSet(int)), &s_global, SLOT(setLevels(int)));
		connect(this, SIGNAL(levelsEnabled(int)), &s_global, SLOT(enableLevels(int)));
		connect(this, SIGNAL(levelsDisabled(int)), &s_global, SLOT(disableLevels(int)));
	}
}

LogController::Stream LogController::operator()(int level) {
	return Stream(this, level);
}

void LogController::postLog(int level, const QString& string) {
	if (!(m_logLevel & level)) {
		return;
	}
	emit logPosted(level, string);
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
	case GBA_LOG_DEBUG:
		return tr("DEBUG");
	case GBA_LOG_STUB:
		return tr("STUB");
	case GBA_LOG_INFO:
		return tr("INFO");
	case GBA_LOG_WARN:
		return tr("WARN");
	case GBA_LOG_ERROR:
		return tr("ERROR");
	case GBA_LOG_FATAL:
		return tr("FATAL");
	case GBA_LOG_GAME_ERROR:
		return tr("GAME ERROR");
	case GBA_LOG_SWI:
		return tr("SWI");
	case GBA_LOG_STATUS:
		return tr("STATUS");
	case GBA_LOG_SIO:
		return tr("SIO");
	}
	return QString();
}

LogController::Stream::Stream(LogController* controller, int level)
	: m_log(controller)
	, m_level(level)
{
}

LogController::Stream::~Stream() {
	m_log->postLog(m_level, m_queue.join(" "));
}

LogController::Stream& LogController::Stream::operator<<(const QString& string) {
	m_queue.append(string);
	return *this;
}
