/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "input/InputSource.h"

namespace QGBA {

class InputDriver;

class KeySource : public InputSource {
Q_OBJECT

public:
	KeySource(InputDriver* driver, QObject* parent = nullptr);

	virtual QSet<int> currentKeys() = 0;
};

}
