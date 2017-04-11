/* Copyright (c) 2013-2016 Jeffrey Pfau
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

#include <mgba/core/thread.h>

#include "InputController.h"
#include "LoadSaveState.h"
#include "LogController.h"
struct mArguments;

namespace QGBA {

class ConfigController;
class DebuggerConsoleController;
class Display;
class GameController;
class GDBController;
class GIFView;
class InputModel;
class LibraryView;
class LogView;
class ShaderSelector;
class VideoView;
class WindowBackground;

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(ConfigController* config, int playerId = 0, QWidget* parent = nullptr);
	virtual ~Window();

	GameController* controller() { return m_controller; }

	void setConfig(ConfigController*);
	void argumentsPassed(mArguments*);

	void resizeFrame(const QSize& size);

signals:
	void startDrawing(mCoreThread*);
	void shutdown();
	void audioBufferSamplesChanged(int samples);
	void sampleRateChanged(unsigned samples);
	void fpsTargetChanged(float target);

public slots:
	void selectROM();
#ifdef USE_SQLITE3
	void selectROMInArchive();
	void addDirToLibrary();
#endif
	void selectSave(bool temporary);
	void selectPatch();
	void enterFullScreen();
	void exitFullScreen();
	void toggleFullScreen();
	void loadConfig();
	void reloadConfig();
	void saveConfig();

	void replaceROM();

	void multiplayerChanged();

	void importSharkport();
	void exportSharkport();

	void openSettingsWindow();
	void openAboutScreen();

#ifdef USE_DEBUGGERS
	void consoleOpen();
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
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void showEvent(QShowEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void focusInEvent(QFocusEvent*) override;
	virtual void focusOutEvent(QFocusEvent*) override;
	virtual void dragEnterEvent(QDragEnterEvent*) override;
	virtual void dropEvent(QDropEvent*) override;
	virtual void mouseMoveEvent(QMouseEvent*) override;
	virtual void mousePressEvent(QMouseEvent*) override;
	virtual void mouseReleaseEvent(QMouseEvent*) override;

private slots:
	void gameStarted(mCoreThread*, const QString&);
	void gameStopped();
	void gameCrashed(const QString&);
	void gameFailed();
	void unimplementedBiosCall(int);

	void tryMakePortable();
	void mustRestart();

	void recordFrame();
	void showFPS();
	void focusCheck();

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

	template <typename T, typename A> std::function<void()> openTView(A arg);
	template <typename T> std::function<void()> openTView();

	QAction* addControlledAction(QMenu* menu, QAction* action, const QString& name);
	QAction* addHiddenAction(QMenu* menu, QAction* action, const QString& name);

	void updateTitle(float fps = -1);

	QString getFilters() const;
	QString getFiltersArchive() const;

	GameController* m_controller;
	Display* m_display;
	int m_savedScale;
	// TODO: Move these to a new class
	QList<QAction*> m_gameActions;
	QList<QAction*> m_nonMpActions;
	QList<QPair<QAction*, int>> m_platformActions;
	QMap<int, QAction*> m_frameSizes;
	LogController m_log;
	LogView* m_logView;
#ifdef USE_DEBUGGERS
	DebuggerConsoleController* m_console;
#endif
	LoadSaveState* m_stateWindow;
	WindowBackground* m_screenWidget;
	QPixmap m_logo;
	ConfigController* m_config;
	InputModel* m_inputModel;
	InputController m_inputController;
	QList<QDateTime> m_frameList;
	QTimer m_fpsTimer;
	QList<QString> m_mruFiles;
	QMenu* m_mruMenu;
	ShaderSelector* m_shaderView;
	bool m_fullscreenOnStart;
	QTimer m_focusCheck;
	bool m_autoresume;
	bool m_wasOpened;

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

#ifdef USE_SQLITE3
	LibraryView* m_libraryView;
#endif
};

class WindowBackground : public QLabel {
Q_OBJECT

public:
	WindowBackground(QWidget* parent = 0);

	void setSizeHint(const QSize& size);
	virtual QSize sizeHint() const override;
	void setLockAspectRatio(int width, int height);
	void setCenteredAspectRatio(int width, int height);

protected:
	virtual void paintEvent(QPaintEvent*) override;

private:
	QSize m_sizeHint;
	bool m_centered;
	int m_aspectWidth;
	int m_aspectHeight;
};

}

#endif
