/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractScrollArea>
#include <QFont>
#include <QSize>
#include <QStaticText>
#include <QVector>

#include <memory>

#include <mgba-util/text-codec.h>

struct mCore;

namespace QGBA {

class CoreController;

class MemoryModel : public QAbstractScrollArea {
Q_OBJECT

public:
	MemoryModel(QWidget* parent = nullptr);

	void setController(std::shared_ptr<CoreController> controller);

	void setRegion(uint32_t base, uint32_t size, const QString& name = QString(), int segment = -1);
	void setSegment(int segment);

	void setAlignment(int);
	int alignment() const { return m_align; }

	QByteArray serialize();
	void deserialize(const QByteArray&);
	QString decodeText(const QByteArray&);

public slots:
	void jumpToAddress(const QString& hex);
	void jumpToAddress(uint32_t);

	void loadTBLFromPath(const QString& path);
	void loadTBL();

	void copy();
	void paste();
	void save();
	void load();

signals:
	void selectionChanged(uint32_t start, uint32_t end);

protected:
	void resizeEvent(QResizeEvent*) override;
	void paintEvent(QPaintEvent*) override;
	void wheelEvent(QWheelEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void keyPressEvent(QKeyEvent*) override;

private:
	void boundsCheck();

	bool isInSelection(uint32_t address);
	bool isEditing(uint32_t address);
	void drawEditingText(QPainter& painter, const QPointF& origin);

	void adjustCursor(int adjust, bool shift);

	class TextCodecFree {
	public:
		void operator()(TextCodec*);
	};

	mCore* m_core = nullptr;
	std::unique_ptr<TextCodec, TextCodecFree> m_codec;
	QFont m_font;
	int m_cellHeight;
	int m_letterWidth;
	uint32_t m_base;
	uint32_t m_size;
	int m_top = 0;
	int m_align = 1;
	QMargins m_margins;
	QVector<QStaticText> m_staticNumbers;
	QVector<QStaticText> m_staticLatin1;
	QStaticText m_regionName;
	QSizeF m_cellSize;
	QPair<uint32_t, uint32_t> m_selection{0, 0};
	uint32_t m_selectionAnchor = 0;
	uint32_t m_buffer;
	int m_bufferedNybbles;
	int m_currentBank;
};

}
