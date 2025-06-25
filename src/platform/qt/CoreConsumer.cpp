/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CoreConsumer.h"

#include "CoreController.h"

using namespace QGBA;

CoreProvider::CoreProvider(std::shared_ptr<CoreController> controller) {
	setController(controller);
}

CoreProvider::~CoreProvider() {
	for (CoreConsumer* consumer : m_consumers) {
		consumer->m_provider = nullptr;
	}
}

void CoreProvider::setController(std::shared_ptr<CoreController> controller) {
	if (m_controller) {
		// Disconnect all signals from old controller
		QObject::disconnect(m_controller.get(), nullptr, this, nullptr);
	}
	std::shared_ptr<CoreController> oldController = m_controller;
	m_controller = controller;
	for (CoreConsumer* consumer : m_consumers) {
		consumer->callControllerChanged(oldController);
	}
}

void CoreProvider::setController(CoreController* controller) {
	setController(std::shared_ptr<CoreController>(controller));
}

CoreController* CoreProvider::get() const {
	return m_controller.get();
}

void CoreProvider::addConsumer(CoreConsumer* consumer) {
	m_consumers.insert(consumer);
}

void CoreProvider::removeConsumer(CoreConsumer* consumer) {
	m_consumers.erase(consumer);
}

void CoreProvider::swap(std::shared_ptr<CoreController>& controller) {
	m_controller.swap(controller);
}

CoreConsumer::CoreConsumer(CoreProvider* provider) {
	setCoreProvider(provider);
}

CoreConsumer::CoreConsumer(const CoreConsumer& other) {
	setCoreProvider(other.m_provider);
}

CoreConsumer::~CoreConsumer() {
	if (m_provider) {
		m_provider->removeConsumer(this);
	}
}

void CoreConsumer::setCoreProvider(CoreProvider* provider) {
	if (m_provider) {
		m_provider->removeConsumer(this);
	}
	m_provider = provider;
	if (provider) {
		provider->addConsumer(this);
	}
}

CoreController* CoreConsumer::controller() const {
	if (!m_provider) {
		return nullptr;
	}
	return m_provider->get();
}

std::shared_ptr<CoreController> CoreConsumer::sharedController() const {
	if (!m_provider) {
		return nullptr;
	}
	return *m_provider;
}

void CoreConsumer::callControllerChanged(std::shared_ptr<CoreController> oldController) {
	if (onControllerChanged) {
		onControllerChanged(oldController);
	}
}

void CoreConsumer::providerDestroyed() {
	m_provider = nullptr;
}
