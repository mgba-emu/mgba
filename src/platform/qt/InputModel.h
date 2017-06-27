/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_MODEL
#define QGBA_INPUT_MODEL

#include <mgba/core/core.h>

#include "InputIndex.h"

#include <QAbstractItemModel>

#include <functional>

class QAction;
class QKeyEvent;
class QMenu;
class QString;

namespace QGBA {

class ConfigController;
class InputIndex;
class InputProfile;

class InputModel : public QAbstractItemModel {
Q_OBJECT

public:
	InputModel(const InputIndex& index, QObject* parent = nullptr);
	InputModel(QObject* parent = nullptr);

	void clone(const InputIndex& index);

	void setProfile(const QString& profile);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	InputItem* itemAt(const QModelIndex& index);
	const InputItem* itemAt(const QModelIndex& index) const;

	InputItem* root() { return m_root.root(); }

private:
	QModelIndex index(InputItem* item, int column = 0) const;

	InputIndex m_root;
	QString m_profileName;
};

}

#endif
