/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_IOVIEWER
#define QGBA_IOVIEWER

#include <QDialog>
#include <QList>

#include "ui_IOViewer.h"

namespace QGBA {

class GameController;

class IOViewer : public QDialog {
Q_OBJECT

public:
	struct RegisterItem {
		RegisterItem(const QString& description, uint start, uint size = 1, bool readonly = false)
			: start(start)
			, size(size)
			, readonly(readonly)
			, description(description) {}
		RegisterItem(const QString& description, uint start, uint size, QStringList items, bool readonly = false)
			: start(start)
			, size(size)
			, readonly(readonly)
			, description(description)
			, items(items) {}
		uint start;
		uint size;
		bool readonly;
		QString description;
		QStringList items;
	};
	typedef QList<RegisterItem> RegisterDescription;

	IOViewer(GameController* controller, QWidget* parent = nullptr);

	static const QList<RegisterDescription>& registerDescriptions();

signals:
	void valueChanged();

public slots:
	void updateRegister();
	void selectRegister(unsigned address);

private slots:
	void buttonPressed(QAbstractButton* button);
	void bitFlipped();
	void writeback();
	void selectRegister();

private:
	static QList<RegisterDescription> s_registers;
	Ui::IOViewer m_ui;

	unsigned m_register;
	uint16_t m_value;

	QCheckBox* m_b[16];

	GameController* m_controller;
};

}

#endif
