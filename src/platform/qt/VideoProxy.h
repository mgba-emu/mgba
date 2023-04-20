/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QMutex>
#include <QObject>
#include <QWaitCondition>

#include <mgba/feature/video-logger.h>
#include <mgba-util/ring-fifo.h>

namespace QGBA {

class CoreController;

class VideoProxy : public QObject {
Q_OBJECT

public:
	VideoProxy();

	void attach(CoreController*);
	void detach(CoreController*);
	void setBlocking(bool block) { m_logger.waitOnFlush = block; }

signals:
	void dataAvailable();

public slots:
	void processData();
	void reset();
	void handleEvent(int);

private:
	void init();
	void deinit();

	bool writeData(const void* data, size_t length);
	bool readData(void* data, size_t length, bool block);
	void postEvent(enum mVideoLoggerEvent event);

	void lock();
	void unlock();
	void wait();
	void wake(int y);

	template<typename T, typename... A> struct callback {
		using type = T (VideoProxy::*)(A...);

		template<type F> static T func(mVideoLogger* logger, A... args) {
			VideoProxy* proxy = static_cast<Logger*>(logger)->p;
			return (proxy->*F)(args...);
		}
	};

	template<void (VideoProxy::*F)()> static void cbind(mVideoLogger* logger) { callback<void>::func<F>(logger); }

	struct Logger : public mVideoLogger {
		VideoProxy* p;
	} m_logger;

	RingFIFO m_dirtyQueue;
	QMutex m_mutex;
	QWaitCondition m_toThreadCond;
	QWaitCondition m_fromThreadCond;
};

}
