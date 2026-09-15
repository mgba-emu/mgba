/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>

#include <memory>

#include "CorePointer.h"

struct mDebuggerModule;

namespace QGBA {

class DebuggerController : public QObject, public CoreConsumer {
Q_OBJECT

public:
	DebuggerController(mDebuggerModule* debugger, QObject* parent = nullptr);

public:
	bool isAttached();

public slots:
	virtual void attach();
	virtual void detach();
	virtual void breakInto();
	virtual void shutdown();

protected:
	virtual void attachInternal();
	virtual void shutdownInternal();

	mDebuggerModule* const m_debugger;

private:
	void onCoreDetached(std::shared_ptr<CoreController>);
	void onCoreAttached(std::shared_ptr<CoreController>);
	bool m_autoattach = false;
};

}
