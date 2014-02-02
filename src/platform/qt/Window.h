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

class GDBController;

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(QWidget* parent = 0);
	static GBAKey mapKey(int qtKey);

signals:
	void startDrawing(const uint32_t*, GBAThread*);
	void shutdown();

public slots:
	void selectROM();

#ifdef USE_GDB_STUB
	void gdbOpen();
#endif

protected:
	virtual void keyPressEvent(QKeyEvent* event);
	virtual void keyReleaseEvent(QKeyEvent* event);
	virtual void closeEvent(QCloseEvent*) override;

private slots:
	void gameStarted(GBAThread*);
	void setupAudio(GBAAudio*);

private:
	void setupMenu(QMenuBar*);
	GameController* m_controller;
	Display* m_display;
	QList<QAction*> m_gameActions;

#ifdef USE_GDB_STUB
	GDBController* m_gdbController;
#endif
};

}

#endif
