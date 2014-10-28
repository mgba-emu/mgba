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

	void setMaxLines(int);

private:
	static const int DEFAULT_LINE_LIMIT = 1000;

	Ui::LogView m_ui;
	int m_logLevel;
	int m_lines;
	int m_lineLimit;

	static QString toString(int level);
	void setLevel(int level) { m_logLevel |= level; }
	void clearLevel(int level) { m_logLevel &= ~level; }

	void clearLine();
};

}

#endif
