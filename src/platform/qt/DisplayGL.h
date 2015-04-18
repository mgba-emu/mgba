/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY_GL
#define QGBA_DISPLAY_GL

#include "Display.h"

#include <QGLWidget>
#include <QThread>
#include <QTimer>

struct GBAThread;

namespace QGBA {

class Painter;
class DisplayGL : public Display {
Q_OBJECT

public:
	DisplayGL(const QGLFormat& format, QWidget* parent = nullptr);

public slots:
	void startDrawing(GBAThread* context) override;
	void stopDrawing() override;
	void pauseDrawing() override;
	void unpauseDrawing() override;
	void forceDraw() override;
	void lockAspectRatio(bool lock) override;
	void filter(bool filter) override;
	void framePosted(const uint32_t*) override;

protected:
	virtual void paintEvent(QPaintEvent*) override {};
	virtual void resizeEvent(QResizeEvent*) override;

private:
	Painter* m_painter;
	bool m_started;
	GBAThread* m_context;
	bool m_lockAspectRatio;
	bool m_filter;
};

class Painter : public QGLWidget {
Q_OBJECT

public:
	Painter(const QGLFormat& format, QWidget* parent);

	void setContext(GBAThread*);
	void setBacking(const uint32_t*);

public slots:
	void forceDraw();
	void draw();
	void start();
	void stop();
	void pause();
	void unpause();
	void resize(const QSize& size);
	void lockAspectRatio(bool lock);
	void filter(bool filter);

protected:
	virtual void initializeGL() override;
	virtual void paintEvent(QPaintEvent*) override {}

private:
	void performDraw();

	QTimer* m_drawTimer;
	GBAThread* m_context;
	const uint32_t* m_backing;
	GLuint m_tex;
	QSize m_size;
	bool m_lockAspectRatio;
	bool m_filter;
};

}

#endif
