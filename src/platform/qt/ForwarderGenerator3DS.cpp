/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ForwarderGenerator3DS.h"

using namespace QGBA;

ForwarderGenerator3DS::ForwarderGenerator3DS()
	: ForwarderGenerator(2)
{
}

QList<QPair<QString, QSize>> ForwarderGenerator3DS::imageTypes() const {
	return {
		{ tr("Icon"), QSize(48, 48) },
		{ tr("Banner"), QSize(256, 128) }
	};
}

bool ForwarderGenerator3DS::rebuild(const QString& source, const QString& target) {
	return false;
}
