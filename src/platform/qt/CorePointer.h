/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <memory>

namespace QGBA {

class CoreConsumer;
class CoreController;
class CorePointerSource;

class CorePointer {
	friend class CorePointerSource;

public:
	CorePointer() = default;
	explicit CorePointer(CoreConsumer* consumer);
	~CorePointer();

	void setSource(CorePointerSource* source);
	inline CorePointerSource* source() const { return m_source; }

	CoreController* get() const;
	std::shared_ptr<CoreController> getShared() const;
	inline operator bool() const { return get(); }
	inline CoreController* operator->() const { return get(); }
	inline operator std::shared_ptr<CoreController>() const { return getShared(); }

private:
	void emitAttach();
	void emitDetach();

	CoreConsumer* m_consumer = nullptr;
	CorePointerSource* m_source = nullptr;
};

class CoreConsumer {
	friend class CorePointer;

public:
	CoreConsumer(CorePointerSource* source = nullptr);
	virtual ~CoreConsumer() {}

	inline void setCoreSource(CorePointerSource* source) { m_controller.setSource(source); }

protected:
	CorePointer m_controller;

	virtual void onCoreDetached(std::shared_ptr<CoreController>);
	virtual void onCoreAttached(std::shared_ptr<CoreController>);
};

}
