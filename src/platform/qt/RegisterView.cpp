/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RegisterView.h"

#include "CoreController.h"
#include "GBAApp.h"

#ifdef M_CORE_GBA
#include <mgba/internal/arm/arm.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/sm83/sm83.h>
#endif

#include <QFormLayout>
#include <QLabel>

using namespace QGBA;

RegisterView::RegisterView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	QFormLayout* layout = new QFormLayout;
	setLayout(layout);

	switch (controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		addRegisters({
			"r0",
			"r1",
			"r2",
			"r3",
			"r4",
			"r5",
			"r6",
			"r7",
			"r8",
			"r9",
			"r10",
			"r11",
			"r12",
			"sp",
			"lr",
			"pc",
			"cpsr",
		});
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		addRegisters({
			"a",
			"f",
			"b",
			"c",
			"d",
			"e",
			"h",
			"l",
			"sp",
			"pc"
		});
		break;
#endif
	default:
		break;
	}
}

void RegisterView::addRegisters(const QStringList& names) {
	QFormLayout* form = static_cast<QFormLayout*>(layout());
	const QFont font = GBAApp::app()->monospaceFont();
	for (const auto& reg : names) {
		QLabel* value = new QLabel;
		value->setTextInteractionFlags(Qt::TextSelectableByMouse);
		value->setFont(font);
		form->addWidget(value);
		m_registers[reg] = value;
		form->addRow(reg, value);
	}
}

void RegisterView::updateRegisters() {
	switch (m_controller->platform()) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		updateRegistersARM();
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		updateRegistersSM83();
		break;
#endif
	default:
		break;
	}
}

#ifdef M_CORE_GBA
void RegisterView::updateRegistersARM() {
	CoreController::Interrupter interrupter(m_controller);
	struct ARMCore* core = static_cast<ARMCore*>(m_controller->thread()->core->cpu);
	m_registers["r0"]->setText(QString("%1").arg((uint32_t) core->gprs[0], 8, 16, QChar('0')).toUpper());
	m_registers["r1"]->setText(QString("%1").arg((uint32_t) core->gprs[1], 8, 16, QChar('0')).toUpper());
	m_registers["r2"]->setText(QString("%1").arg((uint32_t) core->gprs[2], 8, 16, QChar('0')).toUpper());
	m_registers["r3"]->setText(QString("%1").arg((uint32_t) core->gprs[3], 8, 16, QChar('0')).toUpper());
	m_registers["r4"]->setText(QString("%1").arg((uint32_t) core->gprs[4], 8, 16, QChar('0')).toUpper());
	m_registers["r5"]->setText(QString("%1").arg((uint32_t) core->gprs[5], 8, 16, QChar('0')).toUpper());
	m_registers["r6"]->setText(QString("%1").arg((uint32_t) core->gprs[6], 8, 16, QChar('0')).toUpper());
	m_registers["r7"]->setText(QString("%1").arg((uint32_t) core->gprs[7], 8, 16, QChar('0')).toUpper());
	m_registers["r8"]->setText(QString("%1").arg((uint32_t) core->gprs[8], 8, 16, QChar('0')).toUpper());
	m_registers["r9"]->setText(QString("%1").arg((uint32_t) core->gprs[9], 8, 16, QChar('0')).toUpper());
	m_registers["r10"]->setText(QString("%1").arg((uint32_t) core->gprs[10], 8, 16, QChar('0')).toUpper());
	m_registers["r11"]->setText(QString("%1").arg((uint32_t) core->gprs[11], 8, 16, QChar('0')).toUpper());
	m_registers["r12"]->setText(QString("%1").arg((uint32_t) core->gprs[12], 8, 16, QChar('0')).toUpper());
	m_registers["sp"]->setText(QString("%1").arg((uint32_t) core->gprs[ARM_SP], 8, 16, QChar('0')).toUpper());
	m_registers["lr"]->setText(QString("%1").arg((uint32_t) core->gprs[ARM_LR], 8, 16, QChar('0')).toUpper());
	m_registers["pc"]->setText(QString("%1").arg((uint32_t) core->gprs[ARM_PC], 8, 16, QChar('0')).toUpper());
	m_registers["cpsr"]->setText(QString("%1").arg((uint32_t) core->cpsr.packed, 8, 16, QChar('0')).toUpper());
}
#endif

#ifdef M_CORE_GB
void RegisterView::updateRegistersSM83() {
	CoreController::Interrupter interrupter(m_controller);
	struct SM83Core* core = static_cast<SM83Core*>(m_controller->thread()->core->cpu);
	m_registers["a"]->setText(QString("%1").arg((uint8_t) core->a, 2, 16, QChar('0')).toUpper());
	m_registers["f"]->setText(QString("%1").arg((uint8_t) core->f.packed, 2, 16, QChar('0')).toUpper());
	m_registers["b"]->setText(QString("%1").arg((uint8_t) core->b, 2, 16, QChar('0')).toUpper());
	m_registers["c"]->setText(QString("%1").arg((uint8_t) core->c, 2, 16, QChar('0')).toUpper());
	m_registers["d"]->setText(QString("%1").arg((uint8_t) core->d, 2, 16, QChar('0')).toUpper());
	m_registers["e"]->setText(QString("%1").arg((uint8_t) core->e, 2, 16, QChar('0')).toUpper());
	m_registers["h"]->setText(QString("%1").arg((uint8_t) core->h, 2, 16, QChar('0')).toUpper());
	m_registers["l"]->setText(QString("%1").arg((uint8_t) core->l, 2, 16, QChar('0')).toUpper());
	m_registers["sp"]->setText(QString("%1").arg((uint8_t) core->sp, 4, 16, QChar('0')).toUpper());
	m_registers["pc"]->setText(QString("%1").arg((uint8_t) core->pc, 4, 16, QChar('0')).toUpper());
}
#endif
