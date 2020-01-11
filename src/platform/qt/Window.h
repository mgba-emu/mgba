/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAction>
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>
#include <QTimer>

#include <functional>
#include <memory>

#include <mgba/core/core.h>
#include <mgba/core/thread.h>

#include "ActionMapper.h"
#include "InputController.h"
#include "LoadSaveState.h"
#include "LogController.h"
struct mArguments;

namespace QGBA {

class AudioProcessor;
class ConfigController;
class CoreController;
class CoreManager;
class DebuggerConsoleController;
class Display;
class FrameView;
class GDBController;
class GIFView;
class LibraryController;
class LogView;
class OverrideView;
class SensorView;
class ShaderSelector;
class ShortcutController;
class VideoView;
class WindowBackground;

class Window : public QMainWindow {
Q_OBJECT

public:
	Window(CoreManager* manager, ConfigController* config, int playerId = 0, QWidget* parent = nullptr);
	virtual ~Window();

	std::shared_ptr<CoreController> controller() { return m_controller; }

	void setConfig(ConfigController*);
	void argumentsPassed(mArguments*);

	void resizeFrame(const QSize& size);

	void updateMultiplayerStatus(bool canOpenAnother) { m_multiWindow->setEnabled(canOpenAnother); }

signals:
	void startDrawing();
	void shutdown();
	void paused(bool);

public slots:
	void setController(CoreController* controller, const QString& fname);
	void selectROM();
#ifdef USE_SQLITE3
	void selectROMInArchive();
	void addDirToLibrary();
#endif
	void selectSave(bool temporary);
	void selectState(bool load);
	void selectPatch();
	void enterFullScreen();
	void exitFullScreen();
	void toggleFullScreen();
	void loadConfig();
	void reloadConfig();
	void saveConfig();

	void loadCamImage();

	void replaceROM();

	void multiplayerChanged();

	void importSharkport();
	void exportSharkport();

	void openSettingsWindow();

	void startVideoLog();

#ifdef USE_DEBUGGERS
	void consoleOpen();
#endif

#ifdef USE_FFMPEG
	void openVideoWindow();
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
	virtual void hideEvent(QHideEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void focusInEvent(QFocusEvent*) override;
	virtual void focusOutEvent(QFocusEvent*) override;
	virtual void dragEnterEvent(QDragEnterEvent*) override;
	virtual void dropEvent(QDropEvent*) override;
	virtual void mouseDoubleClickEvent(QMouseEvent*) override;

private slots:
	void gameStarted();
	void gameStopped();
	void gameCrashed(const QString&);
	void gameFailed();
	void unimplementedBiosCall(int);

	void reloadAudioDriver();
	void reloadDisplayDriver();

	void tryMakePortable();
	void mustRestart();

	void recordFrame();
	void showFPS();
	void focusCheck();

	void updateFrame();

private:
	static const int FPS_TIMER_INTERVAL = 2000;
	static const int MUST_RESTART_TIMEOUT = 10000;

	void setupMenu(QMenuBar*);
	void openStateWindow(LoadSave);

	void attachWidget(QWidget* widget);
	void detachWidget(QWidget* widget);

	void appendMRU(const QString& fname);
	void clearMRU();
	void updateMRU();

	void openView(QWidget* widget);

	template <typename T, typename... A> std::function<void()> openTView(A... arg);
	template <typename T, typename... A> std::function<void()> openControllerTView(A... arg);

	Action* addGameAction(const QString& visibleName, const QString& name, Action::Function action, const QString& menu = {}, const QKeySequence& = {});
	template<typename T, typename V> Action* addGameAction(const QString& visibleName, const QString& name, T* obj, V (T::*action)(), const QString& menu = {}, const QKeySequence& = {});
	Action* addGameAction(const QString& visibleName, const QString& name, Action::BooleanFunction action, const QString& menu = {}, const QKeySequence& = {});

	void updateTitle(float fps = -1);

	QString getFilters() const;
	QString getFiltersArchive() const;

	CoreManager* m_manager;
	std::shared_ptr<CoreController> m_controller;
	std::unique_ptr<AudioProcessor> m_audioProcessor;

	std::unique_ptr<Display> m_display;
	int m_savedScale;

	// TODO: Move these to a new class
	ActionMapper m_actions;
	QList<Action*> m_gameActions;
	QList<Action*> m_nonMpActions;
#ifdef M_CORE_GBA
	QMultiMap<mPlatform, Action*> m_platformActions;
#endif
	Action* m_multiWindow;
	QMap<int, Action*> m_frameSizes;

	LogController m_log{0};
	LogView* m_logView;
#ifdef USE_DEBUGGERS
	DebuggerConsoleController* m_console = nullptr;
#endif
	LoadSaveState* m_stateWindow = nullptr;
	WindowBackground* m_screenWidget;
	QPixmap m_logo{":/res/mgba-1024.png"};
	ConfigController* m_config;
	InputController m_inputController;
	QList<qint64> m_frameList;
	QElapsedTimer m_frameTimer;
	QTimer m_fpsTimer;
	QTimer m_mustRestart;
	QList<QString> m_mruFiles;
	ShortcutController* m_shortcutController;
#if defined(BUILD_GL) || defined(BUILD_GLES2)
	std::unique_ptr<ShaderSelector> m_shaderView;
#endif
	bool m_fullscreenOnStart = false;
	QTimer m_focusCheck;
	bool m_autoresume = false;
	bool m_wasOpened = false;
	QString m_pendingPatch;
	QString m_pendingState;
	bool m_pendingPause = false;
	bool m_pendingClose = false;

	bool m_hitUnimplementedBiosCall;

	std::unique_ptr<OverrideView> m_overrideView;
	std::unique_ptr<SensorView> m_sensorView;
	FrameView* m_frameView = nullptr;

#ifdef USE_FFMPEG
	VideoView* m_videoView = nullptr;
	GIFView* m_gifView = nullptr;
#endif

#ifdef USE_GDB_STUB
	GDBController* m_gdbController = nullptr;
#endif

#ifdef USE_SQLITE3
	LibraryController* m_libraryView;
#endif
};

class WindowBackground : public QWidget {
Q_OBJECT

public:
	WindowBackground(QWidget* parent = 0);

	void setPixmap(const QPixmap& pixmap);
	void setSizeHint(const QSize& size);
	virtual QSize sizeHint() const override;
	void setDimensions(int width, int height);
	void setLockIntegerScaling(bool lock);
	void setLockAspectRatio(bool lock);
	void filter(bool filter);

	const QPixmap& pixmap() const { return m_pixmap; }

protected:
	virtual void paintEvent(QPaintEvent*) override;

private:
	QPixmap m_pixmap;
	QSize m_sizeHint;
	int m_aspectWidth;
	int m_aspectHeight;
	bool m_lockAspectRatio;
	bool m_lockIntegerScaling;
	bool m_filter;
};

}
