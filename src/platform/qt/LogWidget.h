/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QPlainTextEdit>

namespace QGBA {

class LogHighlighter;

class LogWidget : public QPlainTextEdit {
Q_OBJECT

public:
	static constexpr const char* WARN_PREFIX = "[WARNING] ";
	static constexpr const char* ERROR_PREFIX = "[ERROR] ";
	static constexpr const char* PROMPT_PREFIX = "> ";

	LogWidget(QWidget* parent = nullptr);

	void setNewlineTerminated(bool newlineTerminated) { m_newlineTerminated = newlineTerminated; }

public slots:
	void log(const QString&);
	void warn(const QString&);
	void error(const QString&);
	void echo(const QString&);

private:
	static QTextCharFormat s_warn;
	static QTextCharFormat s_error;
	static QTextCharFormat s_prompt;

	bool m_newlineTerminated = false;
};

}
