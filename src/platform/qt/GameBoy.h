/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QList>
#include <QString>

#ifdef M_CORE_GB
#include <mgba/gb/interface.h>

namespace QGBA {

namespace GameBoy {
	QList<GBModel> modelList();
	QString modelName(GBModel);

	QList<GBMemoryBankControllerType> mbcList();
	QString mbcName(GBMemoryBankControllerType);
}

}
#endif
