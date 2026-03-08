/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QLineEdit>

class QAbstractItemModel;
class QKeyEvent;

namespace QGBA {

class HistoryLineEdit : public QLineEdit {
Q_OBJECT

public:
	HistoryLineEdit(QWidget* parent = nullptr);
	virtual ~HistoryLineEdit() = default;

	int activeIndex() const { return m_historyOffset; }

	void setModel(QAbstractItemModel* model);
	QAbstractItemModel* model() const { return m_model; }

signals:
	void indexChanged(int);
	void linePosted(const QString&);
	void emptyLinePosted();

public slots:
	void setIndex(int);

protected:
	void keyPressEvent(QKeyEvent*) override;

private:
	QAbstractItemModel* m_model = nullptr;
	int m_historyOffset = 0;
};

}
