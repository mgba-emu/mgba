/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>

#include <memory>

#include "CoreConsumer.h"

struct mDebuggerModule;

namespace QGBA {

class CoreController;

class DebuggerController : public QObject {
Q_OBJECT

public:
	DebuggerController(mDebuggerModule* debugger, CoreProvider* provider, QObject* parent = nullptr);

public:
	bool isAttached();
	void setController(std::shared_ptr<CoreController> oldController);

public slots:
	virtual void attach();
	virtual void detach();
	virtual void breakInto();
	virtual void shutdown();

protected:
	virtual void attachInternal();
	virtual void shutdownInternal();

	mDebuggerModule* const m_debugger;
	CorePointer<DebuggerController> m_gameController;

private:
	bool m_autoattach = false;
};

}
