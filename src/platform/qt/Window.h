#ifndef QGBA_WINDOW
#define QGBA_WINDOW

#include <QAudioOutput>
#include <QMainWindow>

extern "C" {
#include "gba.h"
}

#include "GameController.h"
#include "Display.h"

namespace QGBA {

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(QWidget* parent = 0);
	static GBAKey mapKey(int qtKey);

public slots:
	void selectROM();

protected:
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void keyReleaseEvent(QKeyEvent* event);

private slots:
	void gameStarted();
	void setupAudio(GBAAudio*);

private:
	void setupMenu(QMenuBar*);
	GameController* m_controller;
	Display* m_display;
	QList<QAction*> m_gameActions;
};

}

#endif
