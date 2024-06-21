/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoProxy.h"

#include "CoreController.h"

#include <QThread>

using namespace QGBA;

VideoProxy::VideoProxy() {
	mVideoLoggerRendererCreate(&m_logger, false);
	m_logger.p = this;
	m_logger.block = true;
	m_logger.waitOnFlush = true;

	m_logger.init = &cbind<&VideoProxy::init>;
	m_logger.reset = &cbind<&VideoProxy::reset>;
	m_logger.deinit = &cbind<&VideoProxy::deinit>;
	m_logger.lock = &cbind<&VideoProxy::lock>;
	m_logger.unlock = &cbind<&VideoProxy::unlock>;
	m_logger.wait = &cbind<&VideoProxy::wait>;
	m_logger.wake = &callback<void, int>::func<&VideoProxy::wake>;

	m_logger.writeData = &callback<bool, const void*, size_t>::func<&VideoProxy::writeData>;
	m_logger.readData = &callback<bool, void*, size_t, bool>::func<&VideoProxy::readData>;
	m_logger.postEvent = &callback<void, enum mVideoLoggerEvent>::func<&VideoProxy::postEvent>;

	connect(this, &VideoProxy::dataAvailable, this, &VideoProxy::processData);
}

void VideoProxy::attach(CoreController* controller) {
	CoreController::Interrupter interrupter(controller);
	controller->thread()->core->videoLogger = &m_logger;
}

void VideoProxy::detach(CoreController* controller) {
	CoreController::Interrupter interrupter(controller);
	if (controller->thread()->core->videoLogger == &m_logger) {
		controller->thread()->core->videoLogger = nullptr;
	}
}

void VideoProxy::processData() {
	mVideoLoggerRendererRun(&m_logger, false);
	m_fromThreadCond.wakeAll();
}

void VideoProxy::init() {
	RingFIFOInit(&m_dirtyQueue, 0x80000);
}

void VideoProxy::reset() {
	m_mutex.lock();
	RingFIFOClear(&m_dirtyQueue);
	m_toThreadCond.wakeAll();
	m_mutex.unlock();
}

void VideoProxy::deinit() {
	RingFIFODeinit(&m_dirtyQueue);
}

bool VideoProxy::writeData(const void* data, size_t length) {
	while (!RingFIFOWrite(&m_dirtyQueue, data, length)) {
		if (QThread::currentThread() == thread()) {
			// We're on the main thread
			mVideoLoggerRendererRun(&m_logger, false);
		} else {
			emit dataAvailable();
			m_mutex.lock();
			m_toThreadCond.wakeAll();
			m_fromThreadCond.wait(&m_mutex);
			m_mutex.unlock();
		}
	}
	return true;
}

bool VideoProxy::readData(void* data, size_t length, bool block) {
	bool read = false;
	while (true) {
		read = RingFIFORead(&m_dirtyQueue, data, length);
		if (!block || read) {
			break;
		}
		m_mutex.lock();
		m_fromThreadCond.wakeAll();
		m_toThreadCond.wait(&m_mutex);
		m_mutex.unlock();
	}
	return read;
}

void VideoProxy::postEvent(enum mVideoLoggerEvent event) {
	if (QThread::currentThread() == thread()) {
		// We're on the main thread
		handleEvent(event);
	} else {
		QMetaObject::invokeMethod(this, "handleEvent", Qt::BlockingQueuedConnection, Q_ARG(int, event));
	}
}

void VideoProxy::handleEvent(int event) {
	m_mutex.lock();
	m_logger.handleEvent(&m_logger, static_cast<enum mVideoLoggerEvent>(event));
	m_mutex.unlock();
}

void VideoProxy::lock() {
	m_mutex.lock();
}

void VideoProxy::unlock() {
	m_mutex.unlock();
}

void VideoProxy::wait() {
	m_mutex.lock();
	while (RingFIFOSize(&m_dirtyQueue)) {
		if (QThread::currentThread() == thread()) {
			// We're on the main thread
			mVideoLoggerRendererRun(&m_logger, false);
		} else {
			emit dataAvailable();
			m_toThreadCond.wakeAll();
			m_fromThreadCond.wait(&m_mutex, 1);
		}
	}
	m_mutex.unlock();
}

void VideoProxy::wake(int y) {
	if ((y & 15) == 15) {
		emit dataAvailable();
		m_toThreadCond.wakeAll();
	}
}
