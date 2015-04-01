/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY_QT
#define QGBA_DISPLAY_QT

#include "Display.h"

#include <QImage>
#include <QTimer>

struct GBAThread;

namespace QGBA {

class DisplayQt : public Display {
Q_OBJECT

public:
	DisplayQt(QWidget* parent = nullptr);

public slots:
	void startDrawing(const uint32_t* buffer, GBAThread* context);
	void stopDrawing();
	void pauseDrawing();
	void unpauseDrawing();
	void forceDraw();
	void lockAspectRatio(bool lock);
	void filter(bool filter);

protected:
	virtual void paintEvent(QPaintEvent*) override;

private:
	QTimer m_drawTimer;
	GBAThread* m_context;
	QImage m_backing;
	bool m_lockAspectRatio;
	bool m_filter;
};

}

#endif
