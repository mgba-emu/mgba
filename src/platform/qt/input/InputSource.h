/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>
#include <QString>

namespace QGBA {

class InputDriver;

class InputSource : public QObject {
Q_OBJECT

public:
	InputSource(InputDriver* driver, QObject* parent = nullptr);
	virtual ~InputSource() = default;

	InputDriver* driver() { return m_driver; }

	virtual QString name() const = 0;
	virtual QString visibleName() const = 0;

protected:
	InputDriver* const m_driver;
};

}
