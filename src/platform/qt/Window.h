#ifndef QGBA_WINDOW
#define QGBA_WINDOW

#include <QAudioOutput>
#include <QMainWindow>

extern "C" {
#include "gba.h"
}

#include "GDBController.h"
#include "Display.h"
#include "InputController.h"
#include "LoadSaveState.h"

struct GBAOptions;
struct GBAArguments;

namespace QGBA {

class ConfigController;
class GameController;
class LogView;
class VideoView;
class WindowBackground;

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(ConfigController* config, QWidget* parent = nullptr);
	virtual ~Window();

	GameController* controller() { return m_controller; }

	void setConfig(ConfigController*);
	void argumentsPassed(GBAArguments*);

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
	void loadConfig();
	void saveConfig();

	void openKeymapWindow();

#ifdef USE_FFMPEG
	void openVideoWindow();
#endif

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
	ConfigController* m_config;
	InputController m_inputController;

#ifdef USE_FFMPEG
	VideoView* m_videoView;
#endif

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
