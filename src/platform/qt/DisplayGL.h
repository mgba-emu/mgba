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

class EmptyGLWidget : public QGLWidget {
public:
	EmptyGLWidget(const QGLFormat& format, QWidget* parent) : QGLWidget(format, parent) {}

protected:
	void paintEvent(QPaintEvent*) override {}
	void resizeEvent(QResizeEvent*) override {}
};

class PainterGL;
class DisplayGL : public Display {
Q_OBJECT

public:
	DisplayGL(const QGLFormat& format, QWidget* parent = nullptr);
	~DisplayGL();

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
	virtual void paintEvent(QPaintEvent*) override {
		QPainter paint(this);
		paint.fillRect(QRect(QPoint(), size()), Qt::black);
	};
	virtual void resizeEvent(QResizeEvent*) override;

private:
	QGLWidget* m_gl;
	PainterGL* m_painter;
	QThread* m_drawThread;
	GBAThread* m_context;
	bool m_lockAspectRatio;
	bool m_filter;
};

class PainterGL : public QObject {
Q_OBJECT

public:
	PainterGL(QGLWidget* parent);

	void setContext(GBAThread*);

public slots:
	void setBacking(const uint32_t*);
	void forceDraw();
	void draw();
	void start();
	void stop();
	void pause();
	void unpause();
	void resize(const QSize& size);
	void lockAspectRatio(bool lock);
	void filter(bool filter);

private:
	void performDraw();

	QGLWidget* m_gl;
	QThread* m_thread;
	QTimer* m_drawTimer;
	GBAThread* m_context;
	GLuint m_tex;
	QSize m_size;
	bool m_lockAspectRatio;
	bool m_filter;
};

}

#endif
