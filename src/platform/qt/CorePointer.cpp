/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CorePointer.h"

#include "CoreController.h"
#include "CorePointerSource.h"

using namespace QGBA;

CoreConsumer::CoreConsumer(CorePointerSource* source)
	: m_controller(this)
{
	if (source) {
		m_controller.setSource(source);
	}
}

void CoreConsumer::onCoreDetached(std::shared_ptr<CoreController>) {
	// default detach behavior is to do nothing
}

void CoreConsumer::onCoreAttached(std::shared_ptr<CoreController>) {
	// default attach behavior is to do nothing
}

CorePointer::CorePointer(CoreConsumer* consumer) {
	m_consumer = consumer;
}

CorePointer::~CorePointer() {
	if (m_source) {
		m_source->removePointer(this);
	}
}

void CorePointer::setSource(CorePointerSource* source) {
	if (m_source) {
		emitDetach();
		m_source->removePointer(this);
	}
	m_source = source;
	if (source) {
		source->addPointer(this);
		emitAttach();
	}
}

CoreController* CorePointer::get() const {
	if (!m_source) {
		return nullptr;
	}
	return m_source->get();
}

std::shared_ptr<CoreController> CorePointer::getShared() const {
	if (!m_source) {
		return nullptr;
	}
	return *m_source;
}

void CorePointer::emitAttach() {
	if (m_consumer && *m_source) {
		m_consumer->onCoreAttached(*m_source);
	}
}

void CorePointer::emitDetach() {
	if (m_consumer && *m_source) {
		m_consumer->onCoreDetached(*m_source);
	}
}
