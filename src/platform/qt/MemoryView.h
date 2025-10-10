/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QValidator>

#include "CorePointer.h"
#include "MemoryModel.h"
#include "MemoryAccessLogModel.h"

#include "ui_MemoryView.h"

namespace QGBA {

class CorePointerSource;

class IntValidator : public QValidator {
Q_OBJECT

public:
	IntValidator(bool isSigned, QObject* parent = nullptr);

	virtual QValidator::State validate(QString& input, int& pos) const override;
	void setWidth(int bytes) { m_width = bytes; }

private:
	int m_width = 1;
	bool m_signed;
};

class MemoryView : public QWidget, public CoreConsumer {
Q_OBJECT

public:
	MemoryView(CorePointerSource* controller, QWidget* parent = nullptr);

public slots:
	void update();
	void jumpToAddress(uint32_t address);

private slots:
	void setIndex(int);
	void setSegment(int);
	void updateSelection(uint32_t start, uint32_t end);
	void updateStatus();
	void saveRange();

private:
	Ui::MemoryView m_ui;
	IntValidator m_sintValidator{true};
	IntValidator m_uintValidator{false};

	QPair<uint32_t, uint32_t> m_region;
	QPair<uint32_t, uint32_t> m_selection;

#ifdef ENABLE_DEBUGGERS
	MemoryAccessLogModel m_malModel;
#endif
};

}
