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
	RingFIFOInit(&m_dirtyQueue, 0x80000);

	m_logger.writeData = &callback<bool, const void*, size_t>::func<&VideoProxy::writeData>;
	m_logger.readData = &callback<bool, void*, size_t, bool>::func<&VideoProxy::readData>;
	m_logger.postEvent = &callback<void, enum mVideoLoggerEvent>::func<&VideoProxy::postEvent>;

	mVideoProxyBackendInit(&m_backend, nullptr);
	m_backend.context = this;
	m_backend.wakeupCb = [](struct mVideoProxyBackend*, void* context) {
		VideoProxy* self = static_cast<VideoProxy*>(context);
		QMetaObject::invokeMethod(self, "commandAvailable");
	};

	connect(this, &VideoProxy::dataAvailable, this, &VideoProxy::processData);
	connect(this, &VideoProxy::commandAvailable, this, &VideoProxy::processCommands);
}

VideoProxy::~VideoProxy() {
	mVideoProxyBackendDeinit(&m_backend);
	RingFIFODeinit(&m_dirtyQueue);
}

void VideoProxy::attach(CoreController* controller) {
	CoreController::Interrupter interrupter(controller);
	m_logContext = &controller->thread()->logger.d;
	controller->thread()->core->videoLogger = &m_logger;
}

void VideoProxy::detach(CoreController* controller) {
	CoreController::Interrupter interrupter(controller);
	if (controller->thread()->core->videoLogger == &m_logger) {
		m_logContext = nullptr;
		controller->thread()->core->videoLogger = nullptr;
	}
}

void VideoProxy::setProxiedBackend(VideoBackend* backend) {
	// TODO: This needs some safety around it
	m_backend.backend = backend;
}

void VideoProxy::processData() {
	mLogSetThreadLogger(m_logContext);
	mVideoLoggerRendererRun(&m_logger, false);
	m_fromThreadCond.wakeAll();
}

void VideoProxy::processCommands() {
	mLogSetThreadLogger(m_logContext);
	mVideoProxyBackendRun(&m_backend, false);
}

void VideoProxy::init() {
}

void VideoProxy::reset() {
	QMutexLocker locker(&m_mutex);
	RingFIFOClear(&m_dirtyQueue);
	m_toThreadCond.wakeAll();
}

void VideoProxy::deinit() {
}

bool VideoProxy::writeData(const void* data, size_t length) {
	while (!RingFIFOWrite(&m_dirtyQueue, data, length)) {
		if (QThread::currentThread() == thread()) {
			// We're on the main thread
			mLogSetThreadLogger(m_logContext);
			mVideoLoggerRendererRun(&m_logger, false);
		} else {
			emit dataAvailable();
			QMutexLocker locker(&m_mutex);
			m_toThreadCond.wakeAll();
			m_fromThreadCond.wait(&m_mutex);
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
		QMutexLocker locker(&m_mutex);
		m_fromThreadCond.wakeAll();
		m_toThreadCond.wait(&m_mutex);
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
	QMutexLocker locker(&m_mutex);
	mLogSetThreadLogger(m_logContext);
	m_logger.handleEvent(&m_logger, static_cast<enum mVideoLoggerEvent>(event));
}

void VideoProxy::lock() {
	m_mutex.lock();
}

void VideoProxy::unlock() {
	m_mutex.unlock();
}

void VideoProxy::wait() {
	QMutexLocker locker(&m_mutex);
	while (RingFIFOSize(&m_dirtyQueue)) {
		if (QThread::currentThread() == thread()) {
			// We're on the main thread
			mLogSetThreadLogger(m_logContext);
			mVideoLoggerRendererRun(&m_logger, false);
		} else {
			emit dataAvailable();
			m_toThreadCond.wakeAll();
			m_fromThreadCond.wait(&m_mutex, 1);
		}
	}
}

void VideoProxy::wake(int y) {
	if ((y & 15) == 15) {
		emit dataAvailable();
		m_toThreadCond.wakeAll();
	}
}
