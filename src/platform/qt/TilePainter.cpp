/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "TilePainter.h"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>

using namespace QGBA;

TilePainter::TilePainter(QWidget* parent)
	: QWidget(parent)
{
	m_backing = QPixmap(256, 768);
	m_backing.fill(Qt::transparent);
	resize(256, 768);
}

void TilePainter::paintEvent(QPaintEvent* event) {
	QPainter painter(this);
	painter.drawPixmap(QPoint(), m_backing);
}

void TilePainter::resizeEvent(QResizeEvent* event) {
	if (width() / 8 != m_backing.width() / 8) {
		m_backing = QPixmap(width(), (3072 * 8) / (width() / 8));
		m_backing.fill(Qt::transparent);
	}
}

void TilePainter::mousePressEvent(QMouseEvent* event) {
	int x = event->x() / 8;
	int y = event->y() / 8;
	emit indexPressed(y * (width() / 8) + x);
}

void TilePainter::setTile(int index, const uint16_t* data) {
	QPainter painter(&m_backing);
	int w = width() / 8;
	int x = index % w;
	int y = index / w;
	QRect r(x * 8, y * 8, 8, 8);
	QImage tile(reinterpret_cast<const uchar*>(data), 8, 8, QImage::Format_RGB555);
	painter.fillRect(r, tile.rgbSwapped());
	update(r);
}

void TilePainter::setTileCount(int tiles) {
	setMinimumSize(16, (tiles * 8) / (width() / 8));
	resizeEvent(nullptr);
}
