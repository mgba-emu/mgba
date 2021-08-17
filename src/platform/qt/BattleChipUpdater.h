/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "AbstractUpdater.h"

namespace QGBA {

class BattleChipUpdater : public AbstractUpdater {
public:
	BattleChipUpdater(QObject* parent = nullptr);

protected:
	virtual QUrl manifestLocation() const override;
	virtual QUrl parseManifest(const QByteArray&) override;
	virtual QString destination() const override;
};

}
