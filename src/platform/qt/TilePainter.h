/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_TILE_PAINTER
#define QGBA_TILE_PAINTER

#include <QColor>
#include <QWidget>
#include <QVector>

namespace QGBA {

class TilePainter : public QWidget {
Q_OBJECT

public:
	TilePainter(QWidget* parent = nullptr);

public slots:
	void setTile(int index, const uint16_t*);
	void setTileCount(int tiles);
	void setTileMagnification(int mag);

signals:
	void indexPressed(int index);

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

#endif
