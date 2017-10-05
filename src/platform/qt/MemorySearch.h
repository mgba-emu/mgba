/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <memory>

#include "ui_MemorySearch.h"

#include <mgba/core/mem-search.h>

namespace QGBA {

class CoreController;

class MemorySearch : public QWidget {
Q_OBJECT

public:
	static constexpr size_t LIMIT = 10000;

	MemorySearch(std::shared_ptr<CoreController> controller, QWidget* parent = nullptr);
	~MemorySearch();

public slots:
	void refresh();
	void search();
	void searchWithin();

private slots:
	void openMemory();

private:
	bool createParams(mCoreMemorySearchParams*);

	Ui::MemorySearch m_ui;

	std::shared_ptr<CoreController> m_controller;

	mCoreMemorySearchResults m_results;
	QByteArray m_string;
};

}
