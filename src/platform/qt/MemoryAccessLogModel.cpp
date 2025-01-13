/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MemoryAccessLogModel.h"

#include <limits>

using namespace QGBA;

MemoryAccessLogModel::MemoryAccessLogModel(std::weak_ptr<MemoryAccessLogController> controller, mPlatform platform)
	: m_controller(controller)
	, m_platform(platform)
{
}

QVariant MemoryAccessLogModel::data(const QModelIndex& index, int role) const {
	if (role != Qt::DisplayRole) {
		return {};
	}
	if (index.column() != 0) {
		return {};
	}
	int blockIndex = -1;
	int flagIndex = -1;
	QModelIndex parent = index.parent();
	if (!parent.isValid()) {
		blockIndex = index.row();
	} else {
		blockIndex = parent.row();
		flagIndex = index.row();
	}

	if (blockIndex < 0 || blockIndex >= m_cachedBlocks.count()) {
		return {};
	}

	const Block& block = m_cachedBlocks[blockIndex];

	if (flagIndex < 0) {
		return QString("0x%1 – 0x%2")
			.arg(QString("%0").arg(block.region.first, 8, 16, QChar('0')).toUpper())
			.arg(QString("%0").arg(block.region.second, 8, 16, QChar('0')).toUpper());
	}
	for (int i = 0; i < 8; ++i) {
		if (!(block.flags.flags & (1 << i))) {
			continue;
		}
		if (flagIndex == 0) {
			switch (i) {
			case 0:
				return tr("Data read");
			case 1:
				return tr("Data written");
			case 2:
				return tr("Code executed");
			case 3:
				return tr("Code aborted");
			case 4:
				return tr("8-bit access");
			case 5:
				return tr("16-bit access");
			case 6:
				return tr("32-bit access");
			case 7:
				return tr("64-bit access");
			default:
				Q_UNREACHABLE();
			}
		}
		--flagIndex;
	}
	for (int i = 0; i < 16; ++i) {
		if (!(block.flags.flagsEx & (1 << i))) {
			continue;
		}
		if (flagIndex == 0) {
			switch (i) {
			case 0:
				return tr("Accessed by instruction");
			case 1:
				return tr("Accessed by DMA");
			case 2:
				return tr("Accessed by BIOS");
			case 3:
				return tr("Compressed data");
			case 4:
				return tr("Accessed by memory copy");
			case 5:
				return tr("(Unknown extra bit 5)");
			case 6:
				return tr("(Unknown extra bit 6)");
			case 7:
				return tr("(Unknown extra bit 7)");
			case 8:
				return tr("Invalid instruction");
			case 9:
				return tr("Invalid read");
			case 10:
				return tr("Invalid write");
			case 11:
				return tr("Invalid executable address");
			case 12:
				return tr("(Private bit 0)");
			case 13:
				return tr("(Private bit 1)");
			case 14:
				switch (m_platform) {
				case mPLATFORM_GBA:
					return tr("ARM code");
				default:
					return tr("(Private bit 2)");
				}
			case 15:
				switch (m_platform) {
				case mPLATFORM_GBA:
					return tr("Thumb code");
				default:
					return tr("(Private bit 2)");
				}
			default:
				Q_UNREACHABLE();
			}
		}
		--flagIndex;
	}
	return tr("(Unknown)");
}

QModelIndex MemoryAccessLogModel::index(int row, int column, const QModelIndex& parent) const {
	if (column != 0) {
		return {};
	}
	if (parent.isValid()) {
		return createIndex(row, 0, parent.row());
	}
	return createIndex(row, 0, std::numeric_limits<quintptr>::max());
}

QModelIndex MemoryAccessLogModel::parent(const QModelIndex& index) const {
	if (!index.isValid()) {
		return {};
	}
	quintptr row = index.internalId();
	if (row >= std::numeric_limits<uint32_t>::max()) {
		return {};
	}
	return createIndex(row, 0, std::numeric_limits<quintptr>::max());
}

int MemoryAccessLogModel::rowCount(const QModelIndex& parent) const {
	int blockIndex = -1;
	if (!parent.isValid()) {
		return m_cachedBlocks.count();
	} else if (parent.column() != 0) {
		return 0;
	} else if (parent.parent().isValid()) {
		return 0;
	} else {
		blockIndex = parent.row();
	}

	if (blockIndex < 0 || blockIndex >= m_cachedBlocks.count()) {
		return 0;
	}

	const Block& block = m_cachedBlocks[blockIndex];
	return block.flags.count();
}

void MemoryAccessLogModel::updateSelection(uint32_t start, uint32_t end) {
	std::shared_ptr<MemoryAccessLogController> controller = m_controller.lock();
	if (!controller) {
		return;
	}
	QVector<Block> newBlocks;
	uint32_t lastStart = start;
	auto lastFlags = controller->flagsForAddress(m_base + start, m_segment);

	for (uint32_t address = start; address < end; ++address) {
		auto flags = controller->flagsForAddress(m_base + address, m_segment);
		if (flags == lastFlags) {
			continue;
		}
		if (lastFlags) {
			newBlocks.append({ lastFlags, qMakePair(lastStart, address) });
		}
		lastFlags = flags;
		lastStart = address;
	}
	if (lastFlags) {
		newBlocks.append({ lastFlags, qMakePair(lastStart, end) });
	}

	if (m_cachedBlocks.count() == 0 || newBlocks.count() == 0) {
		beginResetModel();
		m_cachedBlocks = newBlocks;
		endResetModel();
		return;
	}

	QPair<int, int> changed{ -1, -1 };
	for (int i = 0; i < m_cachedBlocks.count() && i < newBlocks.count(); ++i) {
		const Block& oldBlock = m_cachedBlocks.at(i);
		const Block& newBlock = newBlocks.at(i);

		if (oldBlock != newBlock) {
			changed = qMakePair(i, m_cachedBlocks.count());
			break;
		}
	}

	if (m_cachedBlocks.count() > newBlocks.count()) {
		beginRemoveRows({}, newBlocks.count(), m_cachedBlocks.count());
		m_cachedBlocks.resize(newBlocks.count());
		endRemoveRows();
		changed.second = newBlocks.count();
	}

	if (m_cachedBlocks.count() < newBlocks.count()) {
		beginInsertRows({}, m_cachedBlocks.count(), newBlocks.count());
		if (changed.first < 0) {
			// Only new rows
			m_cachedBlocks = newBlocks;
			endInsertRows();
			return;
		}
	}

	if (changed.first < 0) {
		// No changed rows, though some might have been removed
		return;
	}

	for (int i = 0; i < m_cachedBlocks.count() && i < newBlocks.count(); ++i) {
		const Block& oldBlock = m_cachedBlocks.at(i);
		const Block& newBlock = newBlocks.at(i);
		if (oldBlock.flags != newBlock.flags) {
			int oldFlags = oldBlock.flags.count();
			int newFlags = newBlock.flags.count();
			if (oldFlags > newFlags) {
				beginRemoveRows(createIndex(i, 0, std::numeric_limits<quintptr>::max()), newFlags, oldFlags);
			} else if (oldFlags < newFlags) {
				beginInsertRows(createIndex(i, 0, std::numeric_limits<quintptr>::max()), oldFlags, newFlags);
			}
			m_cachedBlocks[i] = newBlock;
			emit dataChanged(createIndex(0, 0, i), createIndex(std::min(oldFlags, newFlags), 0, i));
			if (oldFlags > newFlags) {
				endRemoveRows();
			} else if (oldFlags < newFlags) {
				endInsertRows();
			}
		}
	}
	emit dataChanged(createIndex(changed.first, 0, std::numeric_limits<quintptr>::max()),
	                 createIndex(changed.second, 0, std::numeric_limits<quintptr>::max()));

	if (m_cachedBlocks.count() < newBlocks.count()) {
		m_cachedBlocks = newBlocks;
		endInsertRows();
	}
}

void MemoryAccessLogModel::setSegment(int segment) {
	if (m_segment == segment) {
		return;
	}
	beginResetModel();
	m_segment = segment;
	m_cachedBlocks.clear();
	endResetModel();
}

void MemoryAccessLogModel::setRegion(uint32_t base, uint32_t, bool useSegments) {
	if (m_base == base) {
		return;
	}
	beginResetModel();
	m_segment = useSegments ? 0 : -1;
	m_cachedBlocks.clear();
	endResetModel();
}