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
	Display(QWidget* parent = 0);

public slots:
	void startDrawing(const uint32_t* buffer, GBAThread* context);
	void stopDrawing();

protected:
	virtual void paintEvent(QPaintEvent*) {};
	virtual void resizeEvent(QResizeEvent*);

private:
	Painter* m_painter;
	QThread* m_drawThread;
};

class Painter : public QObject {
Q_OBJECT

public:
	Painter(Display* parent);

	void setContext(GBAThread*);
	void setBacking(const uint32_t*);
	void setGLContext(QGLWidget*);
	void resize(const QSize& size);

public slots:
	void draw();
	void start();
	void stop();

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
