/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QColor>
#include <QWidget>
#include <QVector>

#include <mgba/core/interface.h>

namespace QGBA {

class TilePainter : public QWidget {
Q_OBJECT

public:
	TilePainter(QWidget* parent = nullptr);

	QPixmap backing() const { return m_backing; }

public slots:
	void clearTile(int index);
	void setTile(int index, const color_t*);
	void setTileCount(int tiles);
	void setTileMagnification(int mag);

signals:
	void indexPressed(int index);
	void needsRedraw();

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void resizeEvent(QResizeEvent*) override;

private:
	QPixmap m_backing{256, 768};
	int m_size = 8;
	int m_tileCount;
};

}
