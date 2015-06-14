/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Swatch.h"

#include <QMouseEvent>
#include <QPainter>

extern "C" {
#include "gba/video.h"
}

using namespace QGBA;

Swatch::Swatch(QWidget* parent)
	: QWidget(parent)
{
	m_size = 10;
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void Swatch::setSize(int size) {
	m_size = size;
	setDimensions(m_dims);
}

void Swatch::setDimensions(const QSize& size) {
	m_dims = size;
	m_backing = QPixmap(size * (m_size + 1) - QSize(1, 1));
	m_backing.fill(Qt::transparent);
	int elem = size.width() * size.height();
	m_colors.resize(elem);
	for (int i = 0; i < elem; ++i) {
		updateFill(i);
	}
}

void Swatch::setColor(int index, uint16_t color) {
	m_colors[index].setRgb(
		GBA_R8(color),
		GBA_G8(color),
		GBA_B8(color));
	updateFill(index);
}

void Swatch::paintEvent(QPaintEvent* event) {
	QPainter painter(this);
	painter.drawPixmap(QPoint(), m_backing);
}

void Swatch::mousePressEvent(QMouseEvent* event) {
	int x = event->x() / (m_size + 1);
	int y = event->y() / (m_size + 1);
	emit indexPressed(y * m_dims.width() + x);
}

void Swatch::updateFill(int index) {
	QPainter painter(&m_backing);
	int x = index % m_dims.width();
	int y = index / m_dims.width();
	QRect r(x * (m_size + 1), y * (m_size + 1), m_size, m_size);
	painter.fillRect(r, m_colors[index]);
	update(r);
}
