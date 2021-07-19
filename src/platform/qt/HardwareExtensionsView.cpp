/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "HardwareExtensionsView.h"

#include <QWidget>

#include <mgba/internal/gba/hardware-extensions-ids.h>

#include "ConfigController.h"


struct HwExtensionDescription {
	int id;
	const char* description;
};

const struct HwExtensionDescription hwExtensionsDescriptions[] = {
	{ .id = HWEX_ID_MORE_RAM +6, .description = "More RAM" },
};

#define EXTENSIONS_ROWS (sizeof(hwExtensionsDescriptions) / sizeof(hwExtensionsDescriptions[0]))


using namespace QGBA;



HardwareExtensionsView::HardwareExtensionsView(ConfigController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
{
	enabledCounter = 0;
	m_ui.setupUi(this);

	// Add "All" checkbox
	QCheckBox* cb = new QCheckBox(this);
	m_ui.individualEnableTable->setCellWidget(0, 0, cb);
	connect(cb, &QCheckBox::stateChanged, this, [this](int state) {
		QCheckBox* cb;
		for (unsigned i = 0; i < EXTENSIONS_ROWS; i++) {
			cb = (QCheckBox*) m_ui.individualEnableTable->cellWidget(1 + i, 0);
			if (state != cb->checkState()) {
				cb->setCheckState((Qt::CheckState) state);
			}
		}
	});

	// Add the rest of checkboxes
	m_ui.individualEnableTable->setRowCount(1 + EXTENSIONS_ROWS);
	for (unsigned i = 0; i < EXTENSIONS_ROWS; i++) {
		cb = new QCheckBox(this);
		m_ui.individualEnableTable->setVerticalHeaderItem(i + 1, new QTableWidgetItem(tr(hwExtensionsDescriptions[i].description)));
		m_ui.individualEnableTable->setCellWidget(i + 1, 0, cb);
		connect(cb, &QCheckBox::stateChanged, this, [this, i](int state) {
			bool enabled = state == Qt::Checked;
			QCheckBox* cbAll = (QCheckBox*) m_ui.individualEnableTable->cellWidget(0, 0);

			// Update "All" checkbox
			enabledCounter += enabled ? 1 : -1;
			switch (cbAll->checkState()) {
				case Qt::Checked:
					if (enabledCounter < EXTENSIONS_ROWS) {
						cbAll->blockSignals(true);
						cbAll->setCheckState(Qt::Unchecked);
						cbAll->blockSignals(false);
					}
					break;
				default:
					if (enabledCounter == EXTENSIONS_ROWS) {
						cbAll->blockSignals(true);
						cbAll->setCheckState(Qt::Checked);
						cbAll->blockSignals(false);
					}
			}
		});
	}
}

HardwareExtensionsView::~HardwareExtensionsView() {
}
