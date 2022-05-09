/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QTextEdit>

namespace QGBA {

class LogWidget : public QTextEdit {
public:
	LogWidget(QWidget* parent = nullptr);

public slots:
	void log(const QString&);
};

}
