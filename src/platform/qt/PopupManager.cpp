/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "PopupManager.h"

#include <QDialog>

#include "CoreController.h"

using namespace QGBA;
using CorePtr = PopupManagerBase::CorePtr;

PopupManagerBase::PopupManagerBase(PopupManagerBase::Private* d)
: m_d(d)
{
	d->controller.onControllerChanged = [d]{
		d->updateConnections();
		d->notifyWindow();
	};
}

void PopupManagerBase::show() {
	QWidget* w = construct();
	if (!w) {
		return;
	}
	if (d()->isModal) {
		w->setWindowModality(Qt::ApplicationModal);
	}
	w->show();
	w->activateWindow();
	w->raise();
}

QWidget* PopupManagerBase::construct() {
	QWidget* w = d()->window();
	if (w) {
		return w;
	}
	constructImpl();
	w = d()->window();
	if (w && d()->keepAlive) {
		w->setAttribute(Qt::WA_DeleteOnClose);
	}
	d()->updateConnections();
	return w;
}

void PopupManagerBase::Private::setProvider(CoreProvider* provider) {
	controller.setCoreProvider(provider);
	updateConnections();
}

void PopupManagerBase::Private::updateConnections() {
	if (stopConnection) {
		QObject::disconnect(stopConnection);
	}
	CoreController* c = controller.controller();
	QWidget* w = window();
	if (c && w) {
		stopConnection = QObject::connect(c, &CoreController::stopping, w, &QWidget::close);
	}
}
