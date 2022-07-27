/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QMutex>
#include <QObject>
#include <QTextCursor>
#include <QTextDocument>

#include <mgba/core/scripting.h>

namespace QGBA {

class ScriptingTextBuffer : public QObject {
Q_OBJECT

public:
	ScriptingTextBuffer(QObject* parent = nullptr);

	QTextDocument* document() { return &m_document; };
	mScriptTextBuffer* textBuffer() { return &m_shim; }

public slots:
	void setBufferName(const QString&);
	void print(const QString&);
	void clear();
	void setSize(const QSize&);
	void moveCursor(const QPoint&);
	void advance(int);

signals:
	void bufferNameChanged(const QString&);

private:
	struct ScriptingBufferShim : public mScriptTextBuffer {
		ScriptingTextBuffer* p;
		QTextCursor cursor;
	} m_shim;
	QTextDocument m_document;
	QMutex m_mutex;
	QString m_name;

	static void init(struct mScriptTextBuffer*, const char* name);
	static void deinit(struct mScriptTextBuffer*);

	static void setName(struct mScriptTextBuffer*, const char* name);

	static uint32_t getX(const struct mScriptTextBuffer*);
	static uint32_t getY(const struct mScriptTextBuffer*);
	static uint32_t cols(const struct mScriptTextBuffer*);
	static uint32_t rows(const struct mScriptTextBuffer*);

	static void print(struct mScriptTextBuffer*, const char* text);
	static void clear(struct mScriptTextBuffer*);
	static void setSize(struct mScriptTextBuffer*, uint32_t cols, uint32_t rows);
	static void moveCursor(struct mScriptTextBuffer*, uint32_t x, uint32_t y);
	static void advance(struct mScriptTextBuffer*, int32_t);

	QSize m_dims{80, 24};
};

}
