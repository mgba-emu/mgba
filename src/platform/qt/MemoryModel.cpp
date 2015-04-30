/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MemoryModel.h"

#include "GameController.h"

#include <QFontMetrics>
#include <QPainter>
#include <QScrollBar>
#include <QSlider>
#include <QWheelEvent>

extern "C" {
#include "gba/memory.h"
}

using namespace QGBA;

MemoryModel::MemoryModel(QWidget* parent)
	: QAbstractScrollArea(parent)
	, m_cpu(nullptr)
	, m_top(0)
{
	m_font.setFamily("Source Code Pro");
	m_font.setStyleHint(QFont::Monospace);
	m_font.setPointSize(12);
	QFontMetrics metrics(m_font);
	m_cellHeight = metrics.height();
	m_letterWidth = metrics.averageCharWidth();

	for (int i = 0; i < 256; ++i) {
		QStaticText str(QString("%0").arg(i, 2, 16, QChar('0')).toUpper());
		str.prepare(QTransform(), m_font);
		m_staticNumbers.append(str);
	}

	for (int i = 0; i < 128; ++i) {
		QChar c(i);
		if (!c.isPrint()) {
			c = '.';
		}
		QStaticText str = QStaticText(QString(c));
		str.prepare(QTransform(), m_font);
		m_staticAscii.append(str);
	}

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_margins = QMargins(metrics.width("FFFFFF ") + 3, m_cellHeight + 1, metrics.width(" AAAAAAAAAAAAAAAA") + 3, 0);

	verticalScrollBar()->setRange(0, 0x01000001 - viewport()->size().height() / m_cellHeight);
	connect(verticalScrollBar(), &QSlider::sliderMoved, [this](int position) {
		m_top = position;
		update();
	});
	update();
}

void MemoryModel::setController(GameController* controller) {
	m_cpu = controller->thread()->cpu;
}

void MemoryModel::resizeEvent(QResizeEvent*) {
	verticalScrollBar()->setRange(0, 0x01000001 - viewport()->size().height() / m_cellHeight);
	boundsCheck();
}

void MemoryModel::paintEvent(QPaintEvent* event) {
	QPainter painter(viewport());
	painter.setFont(m_font);
	QChar c0('0');
	QSizeF cellSize = QSizeF((viewport()->size().width() - (m_margins.left() + m_margins.right())) / 16.f, m_cellHeight);
	QSizeF letterSize = QSizeF(m_letterWidth, m_cellHeight);
	painter.drawText(QRect(QPoint(0, 0), QSize(m_margins.left(), m_margins.top())), Qt::AlignHCenter, tr("All"));
	painter.drawText(QRect(QPoint(viewport()->size().width() - m_margins.right(), 0), QSize(m_margins.right(), m_margins.top())), Qt::AlignHCenter, tr("ASCII"));
	for (int x = 0; x < 16; ++x) {
		painter.drawText(QRectF(QPointF(cellSize.width() * x + m_margins.left(), 0), cellSize), Qt::AlignHCenter, QString::number(x, 16).toUpper());
	}
	int height = (viewport()->size().height() - m_cellHeight) / m_cellHeight;
	for (int y = 0; y < height; ++y) {
		int yp = m_cellHeight * y + m_margins.top();
		QString data = QString("%0").arg(y + m_top, 6, 16, c0).toUpper();
		painter.drawText(QRectF(QPointF(0, yp), QSizeF(m_margins.left(), m_cellHeight)), Qt::AlignHCenter, data);
		for (int x = 0; x < 16; ++x) {
			uint8_t b = m_cpu->memory.load8(m_cpu, (y + m_top) * 16 + x, nullptr);
			painter.drawStaticText(QPointF(cellSize.width() * (x + 0.5) - m_letterWidth + m_margins.left(), yp), m_staticNumbers[b]);
			painter.drawStaticText(QPointF(viewport()->size().width() - (16 - x) * m_margins.right() / 17.0 - m_letterWidth * 0.5, yp), b < 0x80 ? m_staticAscii[b] : m_staticAscii[0]);
		}
	}
	painter.drawLine(m_margins.left() - 2, 0, m_margins.left() - 2, viewport()->size().height());
	painter.drawLine(viewport()->size().width() - m_margins.right(), 0, viewport()->size().width() - m_margins.right(), viewport()->size().height());
	painter.drawLine(0, m_margins.top(), viewport()->size().width(), m_margins.top());
}

void MemoryModel::wheelEvent(QWheelEvent* event) {
	m_top -= event->angleDelta().y() / 8;
	boundsCheck();
	event->accept();
	update();
	QAbstractScrollArea::wheelEvent(event);
}

void MemoryModel::boundsCheck() {
	if (m_top < 0) {
		m_top = 0;
	} else if (m_top > 0x01000001 - viewport()->size().height() / m_cellHeight) {
		m_top = 0x01000001 - viewport()->size().height() / m_cellHeight;
	}
}
