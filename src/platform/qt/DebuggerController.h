/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>

#include <memory>

struct mDebugger;

namespace QGBA {

class CoreController;

class DebuggerController : public QObject {
Q_OBJECT

public:
	DebuggerController(mDebugger* debugger, QObject* parent = nullptr);

public:
	bool isAttached();
	void setController(std::shared_ptr<CoreController>);

public slots:
	virtual void attach();
	virtual void detach();
	virtual void breakInto();
	virtual void shutdown();

protected:
	virtual void attachInternal();
	virtual void shutdownInternal();

	mDebugger* const m_debugger;
	std::shared_ptr<CoreController> m_gameController;

private:
	bool m_autoattach = false;
};

}
