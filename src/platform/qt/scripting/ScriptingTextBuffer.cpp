/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ScriptingTextBuffer.h"

#include "GBAApp.h"
#include "utils.h"

#include <QMutexLocker>
#include <QPlainTextDocumentLayout>
#include <QTextBlock>

using namespace QGBA;

ScriptingTextBuffer::ScriptingTextBuffer(QObject* parent)
	: QObject(parent)
	, m_document(this)
{
	m_shim.init = &ScriptingTextBuffer::init;
	m_shim.deinit = &ScriptingTextBuffer::deinit;
	m_shim.setName = &ScriptingTextBuffer::setName;
	m_shim.getX = &ScriptingTextBuffer::getX;
	m_shim.getY = &ScriptingTextBuffer::getY;
	m_shim.cols = &ScriptingTextBuffer::cols;
	m_shim.rows = &ScriptingTextBuffer::rows;
	m_shim.print = &ScriptingTextBuffer::print;
	m_shim.clear = &ScriptingTextBuffer::clear;
	m_shim.setSize = &ScriptingTextBuffer::setSize;
	m_shim.moveCursor = &ScriptingTextBuffer::moveCursor;
	m_shim.advance = &ScriptingTextBuffer::advance;
	m_shim.p = this;
	m_shim.cursor = QTextCursor(&m_document);

	auto layout = new QPlainTextDocumentLayout(&m_document);
	m_document.setDocumentLayout(layout);
	m_document.setDefaultFont(GBAApp::app()->monospaceFont());
	m_document.setMaximumBlockCount(m_dims.height());

	QTextOption textOption;
	textOption.setWrapMode(QTextOption::NoWrap);
	m_document.setDefaultTextOption(textOption);
	setBufferName(tr("Untitled buffer"));
}

void ScriptingTextBuffer::setBufferName(const QString& name) {
	m_name = name;
	m_document.setMetaInformation(QTextDocument::DocumentTitle, name);
	emit bufferNameChanged(name);
}

void ScriptingTextBuffer::lineBreak() {
	bool nextBlockExists = m_shim.cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, 1);
	if (!nextBlockExists) {
		m_shim.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor, 1);
		m_shim.cursor.insertBlock();
	}
}

void ScriptingTextBuffer::carriageReturn() {
	m_shim.cursor.movePosition(QTextCursor::StartOfLine, QTextCursor::MoveAnchor, 1);
}

void ScriptingTextBuffer::tab() {
	QTextCursor& cursor = m_shim.cursor;
	int column = cursor.positionInBlock();
	int move = tabStop - (column % tabStop) + 1;
	if (column + move >= m_dims.width()) {
		lineBreak();
	} else if (column + move <= cursor.block().length()) {
		cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, move - 1);
	} else {
		move = column + move - cursor.block().length();
		cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor, 1);
		cursor.insertText(QString(move, ' '));
	}
}

void ScriptingTextBuffer::insertString(const QString& text) {
	QTextCursor& cursor = m_shim.cursor;
	if (cursor.positionInBlock() >= m_dims.width()) {
		lineBreak();
	}
	cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, text.length());
	cursor.insertText(text);
}

void ScriptingTextBuffer::print(const QString& text) {
	QMutexLocker locker(&m_mutex);

	QTextCursor& cursor = m_shim.cursor;
	QString toInsert;

	for (const QChar& ch : text) {
		int column = cursor.positionInBlock();
		if (ch == '\t' || ch == '\n' || ch == '\r' || column + toInsert.length() >= m_dims.width()) {
			insertString(toInsert);
			toInsert.clear();
		}
		switch (ch.unicode()) {
			case '\t':
				tab();
				break;
			case '\n':
				lineBreak();
				break;
			case '\r':
				carriageReturn();
				break;
			default:
				toInsert += ch;
		}
	}
	if (!toInsert.isEmpty()) {
		insertString(toInsert);
	}
}

void ScriptingTextBuffer::clear() {
	QMutexLocker locker(&m_mutex);
	m_document.clear();
	m_document.setMetaInformation(QTextDocument::DocumentTitle, m_name);
	m_shim.cursor = QTextCursor(&m_document);
}

void ScriptingTextBuffer::setSize(const QSize& size) {
	QMutexLocker locker(&m_mutex);
	m_dims = size;
	m_document.setMaximumBlockCount(m_dims.height());
	for (int i = 0; i < m_document.blockCount(); ++i) {
		if (m_document.findBlockByNumber(i).length() - 1 > m_dims.width()) {
			QTextCursor deleter(m_document.findBlockByNumber(i));
			deleter.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, size.width());
			deleter.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
			deleter.removeSelectedText();
		}
	}
}

void ScriptingTextBuffer::moveCursor(const QPoint& pos) {
	QMutexLocker locker(&m_mutex);
	m_shim.cursor.movePosition(QTextCursor::Start);
	int y = pos.y();
	if (y >= m_dims.height()) {
		y = m_dims.height() - 1;
	}
	if (y >= m_document.blockCount()) {
		m_shim.cursor.movePosition(QTextCursor::End);
		while (y >= m_document.blockCount()) {
			m_shim.cursor.insertBlock();
		}
	} else {
		m_shim.cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, y);
	}

	int x = pos.x();
	if (x >= m_dims.width()) {
		x = m_dims.width() - 1;
	}

	if (x >= m_shim.cursor.block().length()) {
		m_shim.cursor.movePosition(QTextCursor::EndOfBlock);
		m_shim.cursor.insertText(QString(x - m_shim.cursor.block().length() + 1, QChar(' ')));
	} else {
		m_shim.cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, x);
	}
}

void ScriptingTextBuffer::advance(int increment) {
	QMutexLocker locker(&m_mutex);
	int x = m_shim.cursor.positionInBlock();
	int y = m_shim.cursor.blockNumber();
	x += increment;
	if (x > 0) {
		y += x / m_dims.width();
		x %= m_dims.width();
	} else if (x < 0) {
		y += (x - m_dims.width() + 1) / m_dims.width();
		x %= m_dims.width();
		if (x) {
			x += m_dims.width();
		}
	}
	locker.unlock();
	moveCursor({x, y});
}

void ScriptingTextBuffer::init(struct mScriptTextBuffer* buffer, const char* name) {
	ScriptingTextBuffer* self = static_cast<ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	if (name) {
		QMetaObject::invokeMethod(self, "setBufferName", Q_ARG(const QString&, QString::fromUtf8(name)));
	}
	QMetaObject::invokeMethod(self, "clear");
}

void ScriptingTextBuffer::deinit(struct mScriptTextBuffer*) {
	// TODO
}

void ScriptingTextBuffer::setName(struct mScriptTextBuffer* buffer, const char* name) {
	ScriptingTextBuffer* self = static_cast<ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	QMetaObject::invokeMethod(self, "setBufferName", Q_ARG(const QString&, QString::fromUtf8(name)));
}

uint32_t ScriptingTextBuffer::getX(const struct mScriptTextBuffer* buffer) {
	const ScriptingBufferShim* self = static_cast<const ScriptingTextBuffer::ScriptingBufferShim*>(buffer);
	QMutexLocker locker(&self->p->m_mutex);
	return self->cursor.positionInBlock();
}

uint32_t ScriptingTextBuffer::getY(const struct mScriptTextBuffer* buffer) {
	const ScriptingBufferShim* self = static_cast<const ScriptingTextBuffer::ScriptingBufferShim*>(buffer);
	QMutexLocker locker(&self->p->m_mutex);
	return self->cursor.blockNumber();
}

uint32_t ScriptingTextBuffer::cols(const struct mScriptTextBuffer* buffer) {
	ScriptingTextBuffer* self = static_cast<const ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	QMutexLocker locker(&self->m_mutex);
	return self->m_dims.width();
}

uint32_t ScriptingTextBuffer::rows(const struct mScriptTextBuffer* buffer) {
	ScriptingTextBuffer* self = static_cast<const ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	QMutexLocker locker(&self->m_mutex);
	return self->m_dims.height();
}

void ScriptingTextBuffer::print(struct mScriptTextBuffer* buffer, const char* text) {
	ScriptingTextBuffer* self = static_cast<ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	QMetaObject::invokeMethod(self, "print", Q_ARG(const QString&, QString::fromUtf8(text)));
}

void ScriptingTextBuffer::clear(struct mScriptTextBuffer* buffer) {
	ScriptingTextBuffer* self = static_cast<ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	QMetaObject::invokeMethod(self, "clear");
}

void ScriptingTextBuffer::setSize(struct mScriptTextBuffer* buffer, uint32_t cols, uint32_t rows) {
	ScriptingTextBuffer* self = static_cast<ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	if (cols > 500) {
		cols = 500;
	}
	if (rows > 10000) {
		rows = 10000;
	}
	QMetaObject::invokeMethod(self, "setSize", Q_ARG(QSize, QSize(cols, rows)));
}

void ScriptingTextBuffer::moveCursor(struct mScriptTextBuffer* buffer, uint32_t x, uint32_t y) {
	ScriptingTextBuffer* self = static_cast<ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	QPoint point(saturateCast<int>(x), saturateCast<int>(y));
	QMetaObject::invokeMethod(self, "moveCursor", Q_ARG(QPoint, point));
}

void ScriptingTextBuffer::advance(struct mScriptTextBuffer* buffer, int32_t adv) {
	ScriptingTextBuffer* self = static_cast<ScriptingTextBuffer::ScriptingBufferShim*>(buffer)->p;
	emit self->advance(adv);
}
