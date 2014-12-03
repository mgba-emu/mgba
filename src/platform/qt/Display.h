/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_DISPLAY
#define QGBA_DISPLAY

#include <QGLWidget>
#include <QThread>
#include <QTimer>

struct GBAThread;

namespace QGBA {

class Painter;
class Display : public QGLWidget {
Q_OBJECT

public:
	Display(QGLFormat format, QWidget* parent = nullptr);

public slots:
	void startDrawing(const uint32_t* buffer, GBAThread* context);
	void stopDrawing();
	void pauseDrawing();
	void unpauseDrawing();
	void forceDraw();
#ifdef USE_PNG
	void screenshot();
#endif

protected:
	virtual void initializeGL() override;
	virtual void paintEvent(QPaintEvent*) override {};
	virtual void resizeEvent(QResizeEvent*) override;

private:
	Painter* m_painter;
	QThread* m_drawThread;
	GBAThread* m_context;
};

class Painter : public QObject {
Q_OBJECT

public:
	Painter(Display* parent);

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

private:
	QTimer* m_drawTimer;
	GBAThread* m_context;
	const uint32_t* m_backing;
	GLuint m_tex;
	QGLWidget* m_gl;
	QSize m_size;
};

}

#endif
