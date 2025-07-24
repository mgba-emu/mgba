/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryModel.h"

#include "../utils.h"

#include <QApplication>
#include <QDir>
#include <QItemSelectionModel>
#include <QSortFilterProxyModel>
#include <QStyle>

#include <algorithm>

using namespace QGBA;

static const QStringList iconSets{
	"GBA",
	"GBC",
	"GB",
	"SGB",
};

static QHash<QString, QIcon> platformIcons;

LibraryModel::LibraryModel(QObject* parent)
	: QAbstractItemModel(parent)
	, m_treeMode(false)
	, m_showFilename(false)
{
	if (platformIcons.isEmpty()) {
		for (const QString& platform : iconSets) {
			QString pathTemplate = QStringLiteral(":/res/%1-icon%2").arg(platform.toLower());
			QIcon icon;
			icon.addFile(pathTemplate.arg("-256.png"), QSize(256, 256));
			icon.addFile(pathTemplate.arg("-128.png"), QSize(128, 128));
			icon.addFile(pathTemplate.arg("-32.png"), QSize(32, 32));
			icon.addFile(pathTemplate.arg("-24.png"), QSize(24, 24));
			icon.addFile(pathTemplate.arg("-16.png"), QSize(16, 16));
			// This will silently and harmlessly fail if QSvgIconEngine isn't compiled in.
			icon.addFile(pathTemplate.arg(".svg"));
			platformIcons[platform] = icon;
		}
	}
}

bool LibraryModel::treeMode() const {
	return m_treeMode;
}

void LibraryModel::setTreeMode(bool tree) {
	if (m_treeMode == tree) {
		return;
	}
	beginResetModel();
	m_treeMode = tree;
	endResetModel();
}

bool LibraryModel::showFilename() const {
	return m_showFilename;
}

void LibraryModel::setShowFilename(bool show) {
	if (m_showFilename == show) {
		return;
	}
	m_showFilename = show;
	if (m_treeMode) {
		int numPaths = m_pathOrder.size();
		for (int i = 0; i < numPaths; i++) {
			QModelIndex parent = index(i, 0);
			emit dataChanged(index(0, 0, parent), index(m_pathIndex[m_pathOrder[i]].size() - 1, 0));
		}
	} else {
		emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
	}
}

void LibraryModel::resetEntries(const QList<LibraryEntry>& items) {
	beginResetModel();
	blockSignals(true);

	m_games.clear();
	m_pathOrder.clear();
	m_pathIndex.clear();
	addEntriesList(items);

	blockSignals(false);
	endResetModel();
}

void LibraryModel::addEntries(const QList<LibraryEntry>& items) {
	if (items.isEmpty()) {
		return;
	} else if (m_treeMode) {
		addEntriesTree(items);
	} else {
		addEntriesList(items);
	}
}

void LibraryModel::addEntryInternal(const LibraryEntry& item) {
	m_gameIndex[item.fullpath] = m_games.size();
	m_games.emplace_back(new LibraryEntry(item));
	if (!m_pathIndex.contains(item.base)) {
		m_pathOrder << item.base;
	}
	m_pathIndex[item.base] << m_games.back().get();
}

void LibraryModel::addEntriesList(const QList<LibraryEntry>& items) {
	beginInsertRows(QModelIndex(), m_games.size(), m_games.size() + items.size() - 1);
	for (const LibraryEntry& item : items) {
		addEntryInternal(item);
	}
	endInsertRows();
}

void LibraryModel::addEntriesTree(const QList<LibraryEntry>& items) {
	QHash<QString, QList<const LibraryEntry*>> byPath;
	QHash<QString, QList<const LibraryEntry*>> newPaths;
	for (const LibraryEntry& item : items) {
		if (m_pathIndex.contains(item.base)) {
			byPath[item.base] << &item;
		} else {
			newPaths[item.base] << &item;
		}
	}

	if (newPaths.size() > 0) {
		beginInsertRows(QModelIndex(), m_pathIndex.size(), m_pathIndex.size() + newPaths.size() - 1);
		for (const QString& base : newPaths.keys()) {
			for (const LibraryEntry* item : newPaths[base]) {
				addEntryInternal(*item);
			}
		}
		endInsertRows();
	}

	for (const QString& base : byPath.keys()) {
		QList<const LibraryEntry*>& pathItems = m_pathIndex[base];
		QList<const LibraryEntry*>& newItems = byPath[base];

		QModelIndex parent = indexForPath(base);
		beginInsertRows(parent, pathItems.size(), pathItems.size() + newItems.size() - 1);
		for (const LibraryEntry* item : newItems) {
			addEntryInternal(*item);
		}
		endInsertRows();
	}
}

void LibraryModel::updateEntries(const QList<LibraryEntry>& items) {
	QHash<QModelIndex, SpanSet> updatedSpans;
	for (const LibraryEntry& item : items) {
		QModelIndex idx = index(item.fullpath);
		Q_ASSERT(idx.isValid());
		int pos = m_gameIndex.value(item.fullpath, -1);
		Q_ASSERT(pos >= 0);
		*m_games[pos] = item;
		updatedSpans[idx.parent()].add(pos);
	}
	for (auto iter = updatedSpans.begin(); iter != updatedSpans.end(); iter++) {
		QModelIndex parent = iter.key();
		SpanSet spans = iter.value();
		spans.merge();
		for (const SpanSet::Span& span : spans.spans) {
			QModelIndex topLeft = index(span.left, 0, parent);
			QModelIndex bottomRight = index(span.right, MAX_COLUMN, parent);
			emit dataChanged(topLeft, bottomRight);
		}
	}
}

void LibraryModel::removeEntries(const QList<QString>& items) {
	SpanSet removedRootSpans, removedGameSpans;
	QHash<QString, SpanSet> removedTreeSpans;
	int firstModifiedIndex = m_games.size();

	// Remove the items from the game index and assemble a span
	// set so that we can later inform the view of which rows
	// were removed in an optimized way.
	for (const QString& item : items) {
		int pos = m_gameIndex.value(item, -1);
		Q_ASSERT(pos >= 0);
		if (pos < firstModifiedIndex) {
			firstModifiedIndex = pos;
		}
		LibraryEntry* entry = m_games[pos].get();
		QModelIndex parent = indexForPath(entry->base);
		Q_ASSERT(!m_treeMode || parent.isValid());
		QList<const LibraryEntry*>& pathItems = m_pathIndex[entry->base];
		int pathPos = pathItems.indexOf(entry);
		Q_ASSERT(pathPos >= 0);
		removedGameSpans.add(pos);
		removedTreeSpans[entry->base].add(pathPos);
		m_gameIndex.remove(item);
	}

	if (!m_treeMode) {
		// If not using a tree view, all entries are root entries.
		removedRootSpans = removedGameSpans;
	}

	// Remove the paths from the path indexes.
	// If it's a tree view, inform the view.
	for (const QString& base : removedTreeSpans.keys()) {
		SpanSet& spanSet = removedTreeSpans[base];
		spanSet.merge();
		QList<const LibraryEntry*>& pathIndex = m_pathIndex[base];
		if (spanSet.spans.size() == 1) {
			SpanSet::Span span = spanSet.spans[0];
			if (span.left == 0 && span.right == pathIndex.size() - 1) {
				if (m_treeMode) {
					removedRootSpans.add(m_pathOrder.indexOf(base));
				} else {
					m_pathIndex.remove(base);
					m_pathOrder.removeAll(base);
				}
				continue;
			}
		}
		QModelIndex parent = indexForPath(base);
		spanSet.sort(true);
		for (const SpanSet::Span& span : spanSet.spans) {
			if (m_treeMode) {
				beginRemoveRows(parent, span.left, span.right);
			}
			pathIndex.erase(pathIndex.begin() + span.left, pathIndex.begin() + span.right + 1);
			if (m_treeMode) {
				endRemoveRows();
			}
		}
	}

	// Remove the games from the backing store and path indexes,
	// and tell the view to remove the root items.
	removedRootSpans.merge();
	removedRootSpans.sort(true);
	for (const SpanSet::Span& span : removedRootSpans.spans) {
		beginRemoveRows(QModelIndex(), span.left, span.right);
		if (m_treeMode) {
			for (int i = span.right; i >= span.left; i--) {
				QString base = m_pathOrder.takeAt(i);
				m_pathIndex.remove(base);
			}
		} else {
			// In list view, remove games from the backing store immediately
			m_games.erase(m_games.begin() + span.left, m_games.begin() + span.right + 1);
		}
		endRemoveRows();
	}
	if (m_treeMode) {
		// In tree view, remove them after cleaning up the path indexes.
		removedGameSpans.merge();
		removedGameSpans.sort(true);
		for (const SpanSet::Span& span : removedGameSpans.spans) {
			m_games.erase(m_games.begin() + span.left, m_games.begin() + span.right + 1);
		}
	}

	// Finally, update the game index for the remaining items.
	for (int i = m_games.size() - 1; i >= firstModifiedIndex; i--) {
		m_gameIndex[m_games[i]->fullpath] = i;
	}
}

QModelIndex LibraryModel::index(const QString& game) const {
	int pos = m_gameIndex.value(game, -1);
	if (pos < 0) {
		return QModelIndex();
	}
	if (m_treeMode) {
		const LibraryEntry& entry = *m_games[pos];
		return createIndex(m_pathIndex[entry.base].indexOf(&entry), 0, m_pathOrder.indexOf(entry.base));
	}
	return createIndex(pos, 0);
}

QModelIndex LibraryModel::index(int row, int column, const QModelIndex& parent) const {
	if (!parent.isValid()) {
		return createIndex(row, column, quintptr(0));
	}
	if (!m_treeMode || parent.internalId() || parent.column() != 0) {
		return QModelIndex();
	}
	return createIndex(row, column, parent.row() + 1);
}

QModelIndex LibraryModel::parent(const QModelIndex& child) const {
	if (!child.isValid() || child.internalId() == 0) {
		return QModelIndex();
	}
	return createIndex(child.internalId() - 1, 0, quintptr(0));
}

int LibraryModel::columnCount(const QModelIndex& parent) const {
	if (!parent.isValid() || (parent.column() == 0 && !parent.parent().isValid())) {
		return MAX_COLUMN + 1;
	}
	return 0;
}

int LibraryModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		if (m_treeMode) {
			if (parent.row() < 0 || parent.row() >= m_pathOrder.size() || parent.column() != 0) {
				return 0;
			}
			return m_pathIndex[m_pathOrder[parent.row()]].size();
		}
		return 0;
	}
	if (m_treeMode) {
		return m_pathOrder.size();
	}
	return m_games.size();
}

QVariant LibraryModel::folderData(const QModelIndex& index, int role) const {
	// Precondition: index and role must have already been validated
	if (role == Qt::DecorationRole) {
		return qApp->style()->standardIcon(QStyle::SP_DirOpenIcon);
	}
	if (role == FullPathRole || (index.column() == COL_LOCATION && role != Qt::DisplayRole)) {
		return m_pathOrder[index.row()];
	}
	if (index.column() == COL_NAME) {
		QString path = m_pathOrder[index.row()];
		return path.section('/', -1);
	}
	return QVariant();
}

bool LibraryModel::validateIndex(const QModelIndex& index) const
{
	if (index.model() != this || index.row() < 0 || index.column() < 0 || index.column() > MAX_COLUMN) {
		// Obviously invalid index
		return false;
	}

	if (index.parent().isValid() && !validateIndex(index.parent())) {
		// Parent index is invalid
		return false;
	}

	if (index.row() >= rowCount(index.parent())) {
		// Row is out of bounds for this level of hierarchy
		return false;
	}

	return true;
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
	switch (role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
		case Qt::TextAlignmentRole:
		case FullPathRole:
			break;
		case Qt::ToolTipRole:
			if (index.column() > COL_LOCATION) {
				return QVariant();
			}
			break;
		case Qt::DecorationRole:
			if (index.column() != COL_NAME) {
				return QVariant();
			}
			break;
		default:
			return QVariant();
	}

	if (!validateIndex(index)) {
		return QVariant();
	}

	if (role == Qt::TextAlignmentRole) {
		return index.column() == COL_SIZE ? (int)(Qt::AlignTrailing | Qt::AlignVCenter) : (int)(Qt::AlignLeading | Qt::AlignVCenter);
	}

	const LibraryEntry* entry = nullptr;
	if (m_treeMode) {
		if (!index.parent().isValid()) {
			return folderData(index, role);
		}
		QString path = m_pathOrder[index.parent().row()];
		entry = m_pathIndex[path][index.row()];
	} else if (!index.parent().isValid() && index.row() < (int)m_games.size()) {
		entry = m_games[index.row()].get();
	}

	if (entry) {
		if (role == FullPathRole) {
			return entry->fullpath;
		}
		switch (index.column()) {
		case COL_NAME:
			if (role == Qt::DecorationRole) {
				return platformIcons.value(entry->displayPlatform(), qApp->style()->standardIcon(QStyle::SP_FileIcon));
			}
			return entry->displayTitle(m_showFilename);
		case COL_LOCATION:
			return QDir::toNativeSeparators(entry->base);
		case COL_PLATFORM:
			return nicePlatformFormat(entry->platform);
		case COL_SIZE:
			return (role == Qt::DisplayRole) ? QVariant(niceSizeFormat(entry->filesize)) : QVariant(int(entry->filesize));
		case COL_CRC32:
			return (role == Qt::DisplayRole) ? QVariant(QStringLiteral("%0").arg(entry->crc32, 8, 16, QChar('0'))) : QVariant(entry->crc32);
		}
	}

	return QVariant();
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		switch (section) {
		case COL_NAME:
			return QApplication::translate("LibraryTree", "Name", nullptr);
		case COL_LOCATION:
			return QApplication::translate("LibraryTree", "Location", nullptr);
		case COL_PLATFORM:
			return QApplication::translate("LibraryTree", "Platform", nullptr);
		case COL_SIZE:
			return QApplication::translate("LibraryTree", "Size", nullptr);
		case COL_CRC32:
			return QApplication::translate("LibraryTree", "CRC32", nullptr);
		};
	}
	return QVariant();
}

QModelIndex LibraryModel::indexForPath(const QString& path) {
	int pos = m_pathOrder.indexOf(path);
	if (pos < 0) {
		pos = m_pathOrder.size();
		beginInsertRows(QModelIndex(), pos, pos);
		m_pathOrder << path;
		m_pathIndex[path] = QList<const LibraryEntry*>();
		endInsertRows();
	}
	if (!m_treeMode) {
		return QModelIndex();
	}
	return index(pos, 0, QModelIndex());
}

QModelIndex LibraryModel::indexForPath(const QString& path) const {
	if (!m_treeMode) {
		return QModelIndex();
	}
	int pos = m_pathOrder.indexOf(path);
	if (pos < 0) {
		return QModelIndex();
	}
	return index(pos, 0, QModelIndex());
}

LibraryEntry LibraryModel::entry(const QString& game) const {
	int pos = m_gameIndex.value(game, -1);
	if (pos < 0) {
		return {};
	}
	return *m_games[pos];
}
