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
	void startDrawing(const uint32_t* buffer, GBAThread* context) override;
	void stopDrawing() override {}
	void pauseDrawing() override {}
	void unpauseDrawing() override {}
	void forceDraw() override { update(); }
	void lockAspectRatio(bool lock) override;
	void filter(bool filter) override;
	void framePosted(const uint32_t*) override { update(); }

protected:
	virtual void paintEvent(QPaintEvent*) override;

private:
	GBAThread* m_context;
	QImage m_backing;
	bool m_lockAspectRatio;
	bool m_filter;
};

}

#endif
