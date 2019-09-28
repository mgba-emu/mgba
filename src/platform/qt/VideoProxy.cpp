/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "VideoProxy.h"

#include "CoreController.h"

using namespace QGBA;

VideoProxy::VideoProxy() {
	mVideoLoggerRendererCreate(&m_logger.d, false);
	m_logger.d.block = true;

	m_logger.d.init = &cbind<&VideoProxy::init>;
	m_logger.d.reset = &cbind<&VideoProxy::reset>;
	m_logger.d.deinit = &cbind<&VideoProxy::deinit>;
	m_logger.d.lock = &cbind<&VideoProxy::lock>;
	m_logger.d.unlock = &cbind<&VideoProxy::unlock>;
	m_logger.d.wait = &cbind<&VideoProxy::wait>;
	m_logger.d.wake = &callback<void, int>::func<&VideoProxy::wake>;

	m_logger.d.writeData = &callback<bool, const void*, size_t>::func<&VideoProxy::writeData>;
	m_logger.d.readData = &callback<bool, void*, size_t, bool>::func<&VideoProxy::readData>;
	m_logger.d.postEvent = &callback<void, enum mVideoLoggerEvent>::func<&VideoProxy::postEvent>;

	connect(this, &VideoProxy::dataAvailable, this, &VideoProxy::processData);
	connect(this, &VideoProxy::eventPosted, this, &VideoProxy::handleEvent);
}

void VideoProxy::attach(CoreController* controller) {
	CoreController::Interrupter interrupter(controller);
	controller->thread()->core->videoLogger = &m_logger.d;
}

void VideoProxy::processData() {
	mVideoLoggerRendererRun(&m_logger.d, false);
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
		emit dataAvailable();
		m_mutex.lock();
		m_toThreadCond.wakeAll();
		m_fromThreadCond.wait(&m_mutex);
		m_mutex.unlock();
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
	emit eventPosted(event);
}

void VideoProxy::handleEvent(int event) {
	m_logger.d.handleEvent(&m_logger.d, static_cast<enum mVideoLoggerEvent>(event));
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
		emit dataAvailable();
		m_toThreadCond.wakeAll();
		m_fromThreadCond.wait(&m_mutex, 1);
	}
	m_mutex.unlock();
}

void VideoProxy::wake(int y) {
	if ((y & 15) == 15) {
		emit dataAvailable();
		m_toThreadCond.wakeAll();
	}
}
