/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>
#include <QList>

#include <mgba/core/core.h>

#include <memory>

#include "ui_IOViewer.h"

namespace QGBA {

class CoreController;

class IOViewer : public QDialog {
Q_OBJECT

public:
	struct RegisterItem {
		RegisterItem(const QString& description, uint start, int size = 1, bool readonly = false)
			: start(start)
			, size(size)
			, readonly(readonly)
			, description(description) {}
		RegisterItem(const QString& description, uint start, int size, QStringList items, bool readonly = false)
			: start(start)
			, size(size)
			, readonly(readonly)
			, description(description)
			, items(items) {}
		uint start;
		int size;
		bool readonly;
		QString description;
		QStringList items;
	};
	typedef QList<RegisterItem> RegisterDescription;

	IOViewer(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);

	static const QList<RegisterDescription>& registerDescriptions(mPlatform);

signals:
	void valueChanged();

public slots:
	void updateRegister();
	void selectRegister(int address);

private slots:
	void buttonPressed(QAbstractButton* button);
	void bitFlipped();
	void writeback();
	void selectRegister();

private:
	static QHash<mPlatform, QList<RegisterDescription>> s_registers;
	Ui::IOViewer m_ui;
	uint32_t m_base;
	int m_width;

	int m_register;
	uint16_t m_value;

	QCheckBox* m_b[16];

	std::shared_ptr<CoreController> m_controller;
};

}
