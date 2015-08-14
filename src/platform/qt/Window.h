/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_WINDOW
#define QGBA_WINDOW

#include <QDateTime>
#include <QList>
#include <QMainWindow>
#include <QTimer>

#include <functional>

extern "C" {
#include "gba/gba.h"
}

#include "GDBController.h"
#include "InputController.h"
#include "LoadSaveState.h"
#include "LogController.h"

struct GBAOptions;
struct GBAArguments;

namespace QGBA {

class ConfigController;
class Display;
class GameController;
class GIFView;
class LogView;
class ShortcutController;
class VideoView;
class WindowBackground;

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(ConfigController* config, int playerId = 0, QWidget* parent = nullptr);
	virtual ~Window();

	GameController* controller() { return m_controller; }

	void setConfig(ConfigController*);
	void argumentsPassed(GBAArguments*);

	void resizeFrame(int width, int height);

signals:
	void startDrawing(GBAThread*);
	void shutdown();
	void audioBufferSamplesChanged(int samples);
	void sampleRateChanged(unsigned samples);
	void fpsTargetChanged(float target);

public slots:
	void selectROM();
	void selectBIOS();
	void selectPatch();
	void enterFullScreen();
	void exitFullScreen();
	void toggleFullScreen();
	void loadConfig();
	void saveConfig();

	void replaceROM();

	void importSharkport();
	void exportSharkport();

	void openKeymapWindow();
	void openSettingsWindow();
	void openShortcutWindow();

	void openOverrideWindow();
	void openSensorWindow();
	void openCheatsWindow();

	void openPaletteWindow();
	void openMemoryWindow();

	void openAboutScreen();

#ifdef BUILD_SDL
	void openGamepadWindow();
#endif

#ifdef USE_FFMPEG
	void openVideoWindow();
#endif

#ifdef USE_MAGICK
	void openGIFWindow();
#endif

#ifdef USE_GDB_STUB
	void gdbOpen();
#endif

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual void keyReleaseEvent(QKeyEvent* event) override;
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void showEvent(QShowEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void focusInEvent(QFocusEvent*) override;
	virtual void focusOutEvent(QFocusEvent*) override;
	virtual void dragEnterEvent(QDragEnterEvent*) override;
	virtual void dropEvent(QDropEvent*) override;
	virtual void mouseDoubleClickEvent(QMouseEvent*) override;

private slots:
	void gameStarted(GBAThread*);
	void gameStopped();
	void gameCrashed(const QString&);
	void gameFailed();
	void unimplementedBiosCall(int);

	void tryMakePortable();
	void mustRestart();

	void recordFrame();
	void showFPS();

private:
	static const int FPS_TIMER_INTERVAL = 2000;
	static const int FRAME_LIST_SIZE = 120;

	void setupMenu(QMenuBar*);
	void openStateWindow(LoadSave);

	void attachWidget(QWidget* widget);
	void detachWidget(QWidget* widget);

	void appendMRU(const QString& fname);
	void updateMRU();

	void openView(QWidget* widget);

	QAction* addControlledAction(QMenu* menu, QAction* action, const QString& name);
	QAction* addHiddenAction(QMenu* menu, QAction* action, const QString& name);

	void updateTitle(float fps = NAN);

	GameController* m_controller;
	Display* m_display;
	QList<QAction*> m_gameActions;
	QMap<int, QAction*> m_frameSizes;
	LogController m_log;
	LogView* m_logView;
	LoadSaveState* m_stateWindow;
	WindowBackground* m_screenWidget;
	QPixmap m_logo;
	ConfigController* m_config;
	InputController m_inputController;
	QList<QDateTime> m_frameList;
	QTimer m_fpsTimer;
	QList<QString> m_mruFiles;
	QMenu* m_mruMenu;
	ShortcutController* m_shortcutController;
	int m_playerId;

	bool m_hitUnimplementedBiosCall;

#ifdef USE_FFMPEG
	VideoView* m_videoView;
#endif

#ifdef USE_MAGICK
	GIFView* m_gifView;
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
	void setLockAspectRatio(int width, int height);

protected:
	virtual void paintEvent(QPaintEvent*) override;

private:
	QSize m_sizeHint;
	int m_aspectWidth;
	int m_aspectHeight;
};

}

#endif
