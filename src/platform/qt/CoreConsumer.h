/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>

#include <memory>
#include <unordered_set>

namespace QGBA {

class CoreController;
class CoreConsumer;

class CoreProvider : public QObject {
public:
	CoreProvider() = default;
	CoreProvider(std::shared_ptr<CoreController> controller);
	virtual ~CoreProvider();

	void addConsumer(CoreConsumer* consumer);
	void removeConsumer(CoreConsumer* consumer);

	void setController(std::shared_ptr<CoreController> controller);
	void setController(CoreController* controller);
	CoreController* get() const;
	inline CoreController* operator->() const { return get(); }
	inline operator std::shared_ptr<CoreController>&() { return m_controller; }
	inline operator bool() const { return get(); }

	void swap(std::shared_ptr<CoreController>& controller);

private:
	std::shared_ptr<CoreController> m_controller;
	std::unordered_set<CoreConsumer*> m_consumers;
};

class CoreConsumer {
	friend class CoreProvider;
public:
	using ControllerCallback = std::function<void()>;

	CoreConsumer() = default;
	CoreConsumer(const CoreConsumer& other);
	CoreConsumer(CoreProvider* provider);
	virtual ~CoreConsumer();

	void setCoreProvider(CoreProvider* provider);
	inline CoreProvider* coreProvider() const { return m_provider; }
	CoreController* controller() const;
	std::shared_ptr<CoreController> sharedController() const;
	inline operator bool() const { return controller(); }

	ControllerCallback onControllerChanged;

private:
	void providerDestroyed();

	CoreProvider* m_provider = nullptr;
};

}
