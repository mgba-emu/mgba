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
	void forceDraw();

protected:
	virtual void initializeGL() override;
	virtual void paintEvent(QPaintEvent*) override {};
	virtual void resizeEvent(QResizeEvent*) override;

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

public slots:
	void forceDraw();
	void draw();
	void start();
	void stop();
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
