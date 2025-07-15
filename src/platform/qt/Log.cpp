/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Log.h"

#include <QLoggingCategory>

mLOG_DEFINE_CATEGORY(QT, "Qt", "platform.qt");

using namespace QGBA;

Log* Log::s_target = nullptr;

Log::Stream Log::log(int level, int category) {
	return Stream(s_target, level, category);
}

void Log::setDefaultTarget(Log* target) {
	s_target = target;
}

Log::Log() {
	// Nothing to do
}

Log::~Log() {
	if (s_target == this) {
		s_target = nullptr;
	}
}

void Log::postLog(int level, int category, const QString& string) {
	QLoggingCategory cat(mLogCategoryName(category));
	switch (level) {
	case mLOG_DEBUG:
	case mLOG_GAME_ERROR:
	case mLOG_STUB:
		qCDebug(cat).noquote() << string;
		return;
	case mLOG_INFO:
		qCInfo(cat).noquote() << string;
		return;
	case mLOG_ERROR:
		qCCritical(cat).noquote() << string;
		return;
	case mLOG_FATAL:
		// qFatal doesn't have a stream API
		qFatal("%s: %s", mLogCategoryName(category), qPrintable(string));
		return;
	case mLOG_WARN:
	default:
		qCWarning(cat).noquote() << string;
		return;
	}
}

Log::Stream::Stream(Log* target, int level, int category)
	: m_level(level)
	, m_category(category)
	, m_log(target)
{
}

Log::Stream::~Stream() {
	if (m_log) {
		m_log->postLog(m_level, m_category, m_queue.join(" "));
	} else {
		Log().postLog(m_level, m_category, m_queue.join(" "));
	}
}

Log::Stream& Log::Stream::operator<<(const QString& string) {
	m_queue.append(string);
	return *this;
}
