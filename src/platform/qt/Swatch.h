/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QColor>
#include <QWidget>
#include <QVector>

namespace QGBA {

class Swatch : public QWidget {
Q_OBJECT

public:
	Swatch(QWidget* parent = nullptr);

	void setDimensions(const QSize&);
	void setSize(int size);

public slots:
	void setColor(int index, uint16_t);
	void setColor(int index, uint32_t);

signals:
	void indexPressed(int index);

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;

private:
	int m_size = 10;
	QVector<QColor> m_colors;
	QPixmap m_backing;
	QSize m_dims;

	void updateFill(int index);
};

}
