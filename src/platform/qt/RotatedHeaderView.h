/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QHeaderView>

namespace QGBA {

class RotatedHeaderView : public QHeaderView {
Q_OBJECT

public:
	RotatedHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);

protected:
	void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
	QSize sectionSizeFromContents(int logicalIndex) const override;
};

}
