/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_MEMORY_MODEL
#define QGBA_MEMORY_MODEL

#include <QAbstractScrollArea>
#include <QFont>
#include <QSize>
#include <QStaticText>
#include <QVector>

struct ARMCore;

namespace QGBA {

class GameController;

class MemoryModel : public QAbstractScrollArea {
Q_OBJECT

public:
	MemoryModel(QWidget* parent = nullptr);

	void setController(GameController* controller);

	void setRegion(uint32_t base, uint32_t size, const QString& name = QString());

protected:
	void resizeEvent(QResizeEvent*) override;
	void paintEvent(QPaintEvent*) override;
	void wheelEvent(QWheelEvent*) override;

private:
	void boundsCheck();

	ARMCore* m_cpu;
	QFont m_font;
	int m_cellHeight;
	int m_letterWidth;
	uint32_t m_base;
	uint32_t m_size;
	int m_top;
	QMargins m_margins;
	QVector<QStaticText> m_staticNumbers;
	QVector<QStaticText> m_staticAscii;
	QStaticText m_regionName;
};

}

#endif
