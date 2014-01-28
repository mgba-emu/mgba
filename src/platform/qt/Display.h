#ifndef QGBA_DISPLAY
#define QGBA_DISPLAY

#include <QGLWidget>

namespace QGBA {

class Display : public QGLWidget {
Q_OBJECT

public:
	Display(QWidget* parent = 0);

protected:
	virtual void initializeGL();

public slots:
	void draw(const QImage& image);
	void paintGL();

private:
	GLuint m_tex;
};

}

#endif
