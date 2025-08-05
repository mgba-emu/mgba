/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogController.h"

#include <QLoggingCategory>
#include <QMessageBox>
#include <QTextStream>

#include "ConfigController.h"

using namespace QGBA;

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
using Qt::endl;
#endif

LogController LogController::s_global(mLOG_ALL);
int LogController::s_qtCat{-1};

static void logMessageHandler(QtMsgType msgType, const QMessageLogContext&, const QString& msg) {
	switch (msgType) {
	case QtDebugMsg:
		LOG(QT, DEBUG) << msg;
		break;
	case QtInfoMsg:
		LOG(QT, INFO) << msg;
		break;
	case QtCriticalMsg:
		LOG(QT, ERROR) << msg;
		break;
	case QtFatalMsg:
		LOG(QT, FATAL) << msg;
		break;
	case QtWarningMsg:
	default:
		LOG(QT, WARN) << msg;
		break;
	}
}

QtMessageHandler LogController::installMessageHandler() {
	return qInstallMessageHandler(logMessageHandler);
}

LogController::LogController(int levels, QObject* parent)
	: QObject(parent)
{
	mLogFilterInit(&m_filter);
	mLogFilterSet(&m_filter, "gba.bios", mLOG_STUB | mLOG_FATAL);
	mLogFilterSet(&m_filter, "core.status", mLOG_ALL & ~mLOG_DEBUG);
	m_filter.defaultLevels = levels;
	s_qtCat = mLogCategoryById("platform.qt");

	if (this == &s_global) {
		setDefaultTarget(this);
	} else {
		connect(&s_global, &LogController::logPosted, this, &LogController::postLog);
		connect(this, static_cast<void (LogController::*)(int)>(&LogController::levelsSet), &s_global, static_cast<void (LogController::*)(int)>(&LogController::setLevels));
		connect(this, static_cast<void (LogController::*)(int)>(&LogController::levelsEnabled), &s_global, static_cast<void (LogController::*)(int)>(&LogController::enableLevels));
		connect(this, static_cast<void (LogController::*)(int)>(&LogController::levelsDisabled), &s_global, static_cast<void (LogController::*)(int)>(&LogController::disableLevels));
	}
}

LogController::~LogController() {
	mLogFilterDeinit(&m_filter);
}

int LogController::levels(int category) const {
	return mLogFilterLevels(&m_filter, category);
}

void LogController::load(const ConfigController* config) {
	mLogFilterLoad(&m_filter, config->config());
	if (!levels(mLogCategoryById("gba.bios"))) {
		mLogFilterSet(&m_filter, "gba.bios", mLOG_STUB | mLOG_FATAL);
	}
	if (!levels(mLogCategoryById("core.status"))) {
		mLogFilterSet(&m_filter, "core.status", mLOG_ALL & ~mLOG_DEBUG);
	}
	setLogFile(config->getOption("logFile"));
	logToStdout(config->getOption("logToStdout").toInt());
	logToFile(config->getOption("logToFile").toInt());
}

void LogController::save(ConfigController* config) const {
	mLogFilterSave(&m_filter, config->config());
}

void LogController::postLog(int level, int category, const QString& string) {
	if (!mLogFilterTest(&m_filter, category, static_cast<mLogLevel>(level))) {
		return;
	}
	if ((m_logToStdout || m_logToFile) && this == &s_global) {
		QString line = tr("[%1] %2: %3").arg(LogController::toString(level)).arg(mLogCategoryName(category)).arg(string);

		if (m_logToStdout) {
			QTextStream out(stdout);
			out << line << endl;
		}
		if (m_logToFile && m_logStream) {
			*m_logStream << line << endl;
		}
	}
	if (category == s_qtCat && level == mLOG_ERROR && this == &s_global) {
		QMessageBox* dialog = new QMessageBox(QMessageBox::Critical, tr("An error occurred"), string, QMessageBox::Ok);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->show();
	}
	emit logPosted(level, category, string);
}

void LogController::setLevels(int levels) {
	m_filter.defaultLevels = levels;
	emit levelsSet(levels);
}

void LogController::enableLevels(int levels) {
	m_filter.defaultLevels |= levels;
	emit levelsEnabled(levels);
}

void LogController::disableLevels(int levels) {
	m_filter.defaultLevels &= ~levels;
	emit levelsDisabled(levels);
}

void LogController::setLevels(int levels, int category) {
	auto id = mLogCategoryId(category);
	mLogFilterSet(&m_filter, id, levels);
	emit levelsSet(levels, category);
}

void LogController::enableLevels(int levels, int category) {
	auto id = mLogCategoryId(category);
	int newLevels = mLogFilterLevels(&m_filter, category) | levels;
	mLogFilterSet(&m_filter, id, newLevels);
	emit levelsEnabled(levels, category);
}

void LogController::disableLevels(int levels, int category) {
	auto id = mLogCategoryId(category);
	int newLevels = mLogFilterLevels(&m_filter, category) & ~levels;
	mLogFilterSet(&m_filter, id, newLevels);
	emit levelsDisabled(levels, category);
}

void LogController::clearLevels(int category) {
	auto id = mLogCategoryId(category);
	mLogFilterReset(&m_filter, id);
}

void LogController::logToFile(bool log) {
	m_logToFile = log;
}

void LogController::logToStdout(bool log) {
	m_logToStdout = log;
}

void LogController::setLogFile(const QString& file) {
	m_logStream.reset();
	if (file.isEmpty()) {
		return;
	}
	m_logFile = std::make_unique<QFile>(file);
	m_logFile->open(QIODevice::Append | QIODevice::Text);
	m_logStream = std::make_unique<QTextStream>(m_logFile.get());
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
