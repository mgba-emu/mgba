/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RotatedHeaderView.h"

#include <QPainter>

using namespace QGBA;

RotatedHeaderView::RotatedHeaderView(Qt::Orientation orientation, QWidget* parent)
	: QHeaderView(orientation, parent)
{
	int margin = 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, 0, this);
	setMinimumSectionSize(fontMetrics().height() + margin);
}

void RotatedHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const {
	painter->save();
	painter->translate(rect.x() + rect.width(), rect.y());
	painter->rotate(90);
	QHeaderView::paintSection(painter, QRect(0, 0, rect.height(), rect.width()), logicalIndex);
	painter->restore();
}

QSize RotatedHeaderView::sectionSizeFromContents(int logicalIndex) const {
	return QHeaderView::sectionSizeFromContents(logicalIndex).transposed();
}