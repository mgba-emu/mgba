/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "SavestateButton.h"

#include <QApplication>
#include <QPainter>

using namespace QGBA;

SavestateButton::SavestateButton(QWidget* parent)
	: QAbstractButton(parent)
{
	setAttribute(Qt::WA_TranslucentBackground);
}

void SavestateButton::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	QRect frame(0, 0, width(), height());
	QRect full(1, 1, width() - 2, height() - 2);
	QPalette palette = QApplication::palette(this);
	painter.setPen(Qt::black);
	QLinearGradient grad(0, 0, 0, 1);
	grad.setCoordinateMode(QGradient::ObjectBoundingMode);
	QColor shadow = palette.color(QPalette::Shadow);
	QColor dark = palette.color(QPalette::Dark);
	shadow.setAlpha(128);
	dark.setAlpha(128);
	grad.setColorAt(0, shadow);
	grad.setColorAt(1, dark);
	painter.setBrush(grad);
	painter.drawRect(frame);
	painter.setPen(Qt::NoPen);
	if (!icon().isNull()) {
		painter.drawPixmap(full, icon().pixmap(full.size()));
	}
	if (hasFocus()) {
		QColor highlight = palette.color(QPalette::Highlight);
		highlight.setAlpha(128);
		painter.fillRect(full, highlight);
	}
	painter.setPen(QPen(palette.text(), 0));
	if (icon().isNull()) {
		painter.drawText(full, Qt::AlignCenter, text());
	} else {
		if (!hasFocus()) {
			painter.setPen(QPen(palette.light(), 0));
			painter.setCompositionMode(QPainter::CompositionMode_Exclusion);
		}
		painter.drawText(full, Qt::AlignHCenter | Qt::AlignBottom, text());
	}
}
