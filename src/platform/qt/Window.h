#ifndef QGBA_WINDOW
#define QGBA_WINDOW

#include <QAudioOutput>
#include <QMainWindow>

extern "C" {
#include "gba.h"
}

#include "GameController.h"
#include "Display.h"

#include "ui_Window.h"

namespace QGBA {

class Window : public QMainWindow, Ui::GBAWindow {
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
	void setupAudio(GBAAudio*);

private:
	GameController* m_controller;
	Display* m_display;
};

}

#endif
