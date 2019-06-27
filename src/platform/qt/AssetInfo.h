/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QVariant>

namespace QGBA {

class CoreController;

class AssetInfo : public QGroupBox {
Q_OBJECT

public:
	AssetInfo(QWidget* parent = nullptr);
	void addCustomProperty(const QString& id, const QString& visibleName);

public slots:
	void setCustomProperty(const QString& id, const QVariant& value);

protected:
	virtual int customLocation(const QString& id = {});

private:
	QHash<QString, QLabel*> m_customProperties;
};

}
