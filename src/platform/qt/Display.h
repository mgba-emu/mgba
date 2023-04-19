/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <mgba-util/common.h>

#include <memory>

#include <QWidget>

#include "MessagePainter.h"

struct VDir;
struct VideoShader;

namespace QGBA {

class ConfigController;
class CoreController;
class VideoProxy;

class Display : public QWidget {
Q_OBJECT

public:
	enum class Driver {
		QT = 0,
		OPENGL = 1,
		OPENGL1 = 2,
	};

	Display(QWidget* parent = nullptr);

	static Display* create(QWidget* parent = nullptr);
	static void setDriver(Driver driver) { s_driver = driver; }

	bool isAspectRatioLocked() const { return m_lockAspectRatio; }
	bool isIntegerScalingLocked() const { return m_lockIntegerScaling; }
	bool hasInterframeBlending() const { return m_interframeBlending; }
	bool isFiltered() const { return m_filter; }
	bool isShowOSD() const { return m_showOSD; }
	bool isShowFrameCounter() const { return m_showFrameCounter; }

	QPoint normalizedPoint(CoreController*, const QPoint& localRef);

	virtual void attach(std::shared_ptr<CoreController>);
	virtual void configure(ConfigController*);
	virtual void startDrawing(std::shared_ptr<CoreController>) = 0;
	virtual bool isDrawing() const = 0;
	virtual bool supportsShaders() const = 0;
	virtual VideoShader* shaders() = 0;
	virtual int framebufferHandle() { return -1; }
	virtual void setVideoScale(int) {}
	virtual void setBackgroundImage(const QImage&) = 0;
	virtual QSize contentSize() const = 0;

	virtual void setVideoProxy(std::shared_ptr<VideoProxy> proxy) { m_videoProxy = proxy; }
	std::shared_ptr<VideoProxy> videoProxy() { return m_videoProxy; }
	
signals:
	void drawingStarted();
	void showCursor();
	void hideCursor();

public slots:
	virtual void stopDrawing() = 0;
	virtual void pauseDrawing() = 0;
	virtual void unpauseDrawing() = 0;
	virtual void forceDraw() = 0;
	virtual void lockAspectRatio(bool lock);
	virtual void lockIntegerScaling(bool lock);
	virtual void interframeBlending(bool enable);
	virtual void showOSDMessages(bool enable);
	virtual void showFrameCounter(bool enable);
	virtual void filter(bool filter);
	virtual void swapInterval(int interval) = 0;
	virtual void framePosted() = 0;
	virtual void setShaders(struct VDir*) = 0;
	virtual void clearShaders() = 0;
	virtual void resizeContext() = 0;

	void showMessage(const QString& message);

protected:
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void mouseMoveEvent(QMouseEvent*) override;

	MessagePainter* messagePainter() { return &m_messagePainter; }

private:
	static Driver s_driver;
	static const int MOUSE_DISAPPEAR_TIMER = 1000;

	MessagePainter m_messagePainter;
	bool m_showOSD = true;
	bool m_showFrameCounter = false;
	bool m_lockAspectRatio = false;
	bool m_lockIntegerScaling = false;
	bool m_interframeBlending = false;
	bool m_filter = false;
	QTimer m_mouseTimer;
	std::shared_ptr<VideoProxy> m_videoProxy;
};

}
