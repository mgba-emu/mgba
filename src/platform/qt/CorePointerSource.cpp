/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CorePointerSource.h"

#include "CoreController.h"
#include "CorePointer.h"

using namespace QGBA;

/**
 * CorePointerSource is a layer of indirection around a pointer to CoreController.
 * CorePointer instances can attach to a CorePointerSource in order to always refer
 * to the same underlying CoreController instance as the source, even if the
 * controller is replaced.
 */

CorePointerSource::CorePointerSource(std::shared_ptr<CoreController> controller) {
	setController(controller);
}

CorePointerSource::~CorePointerSource() {
	for (CorePointer* consumer : m_pointers) {
		consumer->m_source = nullptr;
	}
}

void CorePointerSource::setController(std::shared_ptr<CoreController> controller) {
	if (controller == m_controller) {
		return;
	}
	for (CorePointer* ptr : m_pointers) {
		ptr->emitDetach();
	}
	m_controller = controller;
	for (CorePointer* ptr : m_pointers) {
		ptr->emitAttach();
	}
}

CoreController* CorePointerSource::get() const {
	return m_controller.get();
}

void CorePointerSource::addPointer(CorePointer* consumer) {
	m_pointers.insert(consumer);
}

void CorePointerSource::removePointer(CorePointer* consumer) {
	m_pointers.erase(consumer);
}

void CorePointerSource::swap(std::shared_ptr<CoreController>& controller) {
	m_controller.swap(controller);
}

CorePointerSource& CorePointerSource::operator=(const std::shared_ptr<CoreController>& controller) {
	setController(controller);
	return *this;
}
