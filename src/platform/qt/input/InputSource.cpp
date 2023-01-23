/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "input/InputSource.h"

using namespace QGBA;

InputSource::InputSource(InputDriver* driver, QObject* parent)
	: QObject(parent)
	, m_driver(driver)
{
}
