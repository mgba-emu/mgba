/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <memory>
#include <unordered_set>

namespace QGBA {

class CoreController;
class CorePointer;

class CorePointerSource {
public:
	CorePointerSource() = default;
	CorePointerSource(std::shared_ptr<CoreController> controller);
	virtual ~CorePointerSource();

	void addPointer(CorePointer* ptr);
	void removePointer(CorePointer* ptr);

	void setController(std::shared_ptr<CoreController> controller);
	CoreController* get() const;
	inline CoreController* operator->() const { return get(); }
	inline operator std::shared_ptr<CoreController>&() { return m_controller; }
	inline operator bool() const { return get(); }
	CorePointerSource& operator=(const std::shared_ptr<CoreController>& controller);

	void swap(std::shared_ptr<CoreController>& controller);

private:
	std::shared_ptr<CoreController> m_controller;
	std::unordered_set<CorePointer*> m_pointers;
};

}
