/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "IOViewer.h"

#include "GameController.h"

#include <QFontDatabase>

extern "C" {
#include "gba/io.h"
}

using namespace QGBA;

IOViewer::IOViewer(GameController* controller, QWidget* parent)
	: QDialog(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	for (unsigned i = 0; i < REG_MAX >> 1; ++i) {
		const char* reg = GBAIORegisterNames[i];
		if (!reg) {
			continue;
		}
		m_ui.regSelect->addItem(QString::asprintf("0x0400%04X: %s", i << 1, reg), i << 1);
	}

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	m_ui.regValue->setFont(font);

	connect(m_ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonPressed(QAbstractButton*)));
	connect(m_ui.buttonBox, SIGNAL(rejected()), this, SLOT(close()));
	connect(m_ui.regSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(selectRegister()));

	connect(m_ui.b0, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b1, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b2, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b3, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b4, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b5, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b6, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b7, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b8, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.b9, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.bA, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.bB, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.bC, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.bD, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.bE, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	connect(m_ui.bF, SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));

	selectRegister(0);
}

void IOViewer::update() {
	m_value = 0;
	m_controller->threadInterrupt();
	if (m_controller->isLoaded()) {
		m_value = GBAIORead(m_controller->thread()->gba, m_register);
	}
	m_controller->threadContinue();

	m_ui.regValue->setText(QString::asprintf("0x%04X", m_value));
	bool signalsBlocked;
	signalsBlocked = m_ui.b0->blockSignals(true);
	m_ui.b0->setChecked(m_value & 0x0001 ? Qt::Checked : Qt::Unchecked);
	m_ui.b0->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b1->blockSignals(true);
	m_ui.b1->setChecked(m_value & 0x0002 ? Qt::Checked : Qt::Unchecked);
	m_ui.b1->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b2->blockSignals(true);
	m_ui.b2->setChecked(m_value & 0x0004 ? Qt::Checked : Qt::Unchecked);
	m_ui.b2->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b3->blockSignals(true);
	m_ui.b3->setChecked(m_value & 0x0008 ? Qt::Checked : Qt::Unchecked);
	m_ui.b3->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b4->blockSignals(true);
	m_ui.b4->setChecked(m_value & 0x0010 ? Qt::Checked : Qt::Unchecked);
	m_ui.b4->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b5->blockSignals(true);
	m_ui.b5->setChecked(m_value & 0x0020 ? Qt::Checked : Qt::Unchecked);
	m_ui.b5->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b6->blockSignals(true);
	m_ui.b6->setChecked(m_value & 0x0040 ? Qt::Checked : Qt::Unchecked);
	m_ui.b6->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b7->blockSignals(true);
	m_ui.b7->setChecked(m_value & 0x0080 ? Qt::Checked : Qt::Unchecked);
	m_ui.b7->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b8->blockSignals(true);
	m_ui.b8->setChecked(m_value & 0x0100 ? Qt::Checked : Qt::Unchecked);
	m_ui.b8->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.b9->blockSignals(true);
	m_ui.b9->setChecked(m_value & 0x0200 ? Qt::Checked : Qt::Unchecked);
	m_ui.b9->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.bA->blockSignals(true);
	m_ui.bA->setChecked(m_value & 0x0400 ? Qt::Checked : Qt::Unchecked);
	m_ui.bA->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.bB->blockSignals(true);
	m_ui.bB->setChecked(m_value & 0x0800 ? Qt::Checked : Qt::Unchecked);
	m_ui.bB->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.bC->blockSignals(true);
	m_ui.bC->setChecked(m_value & 0x1000 ? Qt::Checked : Qt::Unchecked);
	m_ui.bC->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.bD->blockSignals(true);
	m_ui.bD->setChecked(m_value & 0x2000 ? Qt::Checked : Qt::Unchecked);
	m_ui.bD->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.bE->blockSignals(true);
	m_ui.bE->setChecked(m_value & 0x4000 ? Qt::Checked : Qt::Unchecked);
	m_ui.bE->blockSignals(signalsBlocked);
	signalsBlocked = m_ui.bF->blockSignals(true);
	m_ui.bF->setChecked(m_value & 0x8000 ? Qt::Checked : Qt::Unchecked);
	m_ui.bF->blockSignals(signalsBlocked);
}

void IOViewer::bitFlipped() {
	m_value = 0;
	m_value |= m_ui.b0->isChecked() << 0x0;
	m_value |= m_ui.b1->isChecked() << 0x1;
	m_value |= m_ui.b2->isChecked() << 0x2;
	m_value |= m_ui.b3->isChecked() << 0x3;
	m_value |= m_ui.b4->isChecked() << 0x4;
	m_value |= m_ui.b5->isChecked() << 0x5;
	m_value |= m_ui.b6->isChecked() << 0x6;
	m_value |= m_ui.b7->isChecked() << 0x7;
	m_value |= m_ui.b8->isChecked() << 0x8;
	m_value |= m_ui.b9->isChecked() << 0x9;
	m_value |= m_ui.bA->isChecked() << 0xA;
	m_value |= m_ui.bB->isChecked() << 0xB;
	m_value |= m_ui.bC->isChecked() << 0xC;
	m_value |= m_ui.bD->isChecked() << 0xD;
	m_value |= m_ui.bE->isChecked() << 0xE;
	m_value |= m_ui.bF->isChecked() << 0xF;
	m_ui.regValue->setText(QString::asprintf("0x%04X", m_value));
}

void IOViewer::writeback() {
	m_controller->threadInterrupt();
	if (m_controller->isLoaded()) {
		GBAIOWrite(m_controller->thread()->gba, m_register, m_value);
	}
	m_controller->threadContinue();
	update();
}

void IOViewer::selectRegister(unsigned address) {
	m_register = address;
	update();
}

void IOViewer::selectRegister() {
	selectRegister(m_ui.regSelect->currentData().toUInt());
}

void IOViewer::buttonPressed(QAbstractButton* button) {
	switch (m_ui.buttonBox->standardButton(button)) {
	case QDialogButtonBox::Reset:
	 	update();
	 	break;
	case QDialogButtonBox::Apply:
	 	writeback();
	 	break;
	default:
		break;
	}
}
