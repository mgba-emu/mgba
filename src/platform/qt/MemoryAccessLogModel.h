/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractItemModel>
#include <QVector>

#include "MemoryAccessLogController.h"

struct mCheatDevice;
struct mCheatSet;

namespace QGBA {

class MemoryAccessLogModel : public QAbstractItemModel {
Q_OBJECT

public:
	MemoryAccessLogModel(std::weak_ptr<MemoryAccessLogController> controller, mPlatform platform);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& = QModelIndex()) const override { return 1; }
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

public slots:
	void updateSelection(uint32_t start, uint32_t end);
	void update();
	void setSegment(int segment);
	void setRegion(uint32_t base, uint32_t segmentSize, bool useSegments);

private:
	struct Block {
		MemoryAccessLogController::Flags flags;
		QPair<uint32_t, uint32_t> region;

		bool operator==(const Block& other) const { return flags == other.flags && region == other.region; }
		bool operator!=(const Block& other) const { return flags != other.flags || region != other.region; }
	};

	int flagCount(int index) const;

	std::weak_ptr<MemoryAccessLogController> m_controller;
	mPlatform m_platform;
	uint32_t m_base = 0;
	int m_segment = -1;
	QVector<Block> m_cachedBlocks;
	uint32_t m_start = 0, m_end = 0;
};

}
