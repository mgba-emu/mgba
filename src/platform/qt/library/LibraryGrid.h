/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QListWidget>

#include "LibraryController.h"

namespace QGBA {

class LibraryGrid final : public AbstractGameList {
public:
	explicit LibraryGrid(LibraryController* parent = nullptr);
	~LibraryGrid();

	// AbstractGameList stuff
	virtual mLibraryEntry* selectedEntry() override;
	virtual void selectEntry(mLibraryEntry* game) override;

	virtual void setViewStyle(LibraryStyle newStyle) override;

	virtual void addEntry(mLibraryEntry* item) override;
	virtual void removeEntry(mLibraryEntry* entry) override;

	virtual QWidget* widget() override { return m_widget; }

signals:
	void startGame();

private:
	QListWidget* m_widget;

	// Game banner image size
	const quint32 GRID_BANNER_WIDTH = 320;
	const quint32 GRID_BANNER_HEIGHT = 240;

	const quint32 ICON_BANNER_WIDTH = 64;
	const quint32 ICON_BANNER_HEIGHT = 64;

	QHash<mLibraryEntry*, QListWidgetItem*> m_items;
	LibraryStyle m_currentStyle;
};

}
