#ifndef QGBA_WINDOW
#define QGBA_WINDOW

#include <QAudioOutput>
#include <QMainWindow>

extern "C" {
#include "gba.h"
}

#include "Display.h"
#include "LoadSaveState.h"

struct StartupOptions;

namespace QGBA {

class GameController;
class GDBController;
class LogView;
class WindowBackground;

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(QWidget* parent = nullptr);
	virtual ~Window();

	GameController* controller() { return m_controller; }

	static GBAKey mapKey(int qtKey);

	void optionsPassed(StartupOptions*);

signals:
	void startDrawing(const uint32_t*, GBAThread*);
	void shutdown();
	void audioBufferSamplesChanged(int samples);
	void fpsTargetChanged(float target);

public slots:
	void selectROM();
	void selectBIOS();
	void selectPatch();
	void toggleFullScreen();

#ifdef USE_GDB_STUB
	void gdbOpen();
#endif

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual void keyReleaseEvent(QKeyEvent* event) override;
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;

private slots:
	void gameStarted(GBAThread*);
	void gameStopped();
	void redoLogo();

private:
	void setupMenu(QMenuBar*);
	void openStateWindow(LoadSave);

	void attachWidget(QWidget* widget);
	void detachWidget(QWidget* widget);

	GameController* m_controller;
	Display* m_display;
	QList<QAction*> m_gameActions;
	LogView* m_logView;
	LoadSaveState* m_stateWindow;
	WindowBackground* m_screenWidget;
	QPixmap m_logo;

#ifdef USE_GDB_STUB
	GDBController* m_gdbController;
#endif
};

class WindowBackground : public QLabel {
Q_OBJECT

public:
	WindowBackground(QWidget* parent = 0);

	void setSizeHint(const QSize& size);
	virtual QSize sizeHint() const override;

private:
	QSize m_sizeHint;
};

}

#endif
