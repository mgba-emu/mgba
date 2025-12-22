/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "scripting/AutorunScriptModel.h"

#include "Log.h"

QDataStream& operator<<(QDataStream& stream, const QGBA::AutorunScriptModel::ScriptInfo& object) {
	int restoreVersion = stream.version();
	stream.setVersion(QDataStream::Qt_5_0);
	stream << QGBA::AutorunScriptModel::ScriptInfo::VERSION;
	stream << object.filename.toUtf8();
	stream << object.active;
	stream.setVersion(restoreVersion);
	return stream;
}

QDataStream& operator>>(QDataStream& stream, QGBA::AutorunScriptModel::ScriptInfo& object) {
	static bool displayedError = false;
	int restoreVersion = stream.version();
	stream.setVersion(QDataStream::Qt_5_0);
	uint16_t version = 0;
	stream >> version;
	if (version == 1) {
		QByteArray filename;
		stream >> filename;
		object.filename = QString::fromUtf8(filename);
	} else {
		QString logMessage = QGBA::AutorunScriptModel::tr("Could not load autorun script settings: unknown script info format %1").arg(version);
		if (displayedError) {
			LOG(QT, WARN) << logMessage;
		} else {
			LOG(QT, ERROR) << logMessage;
			displayedError = true;
		}
		stream.setStatus(QDataStream::ReadCorruptData);
		stream.setVersion(restoreVersion);
		return stream;
	}
	stream >> object.active;
	stream.setVersion(restoreVersion);
	return stream;
}

using namespace QGBA;

void AutorunScriptModel::registerMetaTypes() {
	qRegisterMetaType<AutorunScriptModel::ScriptInfo>();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
	qRegisterMetaTypeStreamOperators<AutorunScriptModel::ScriptInfo>("QGBA::AutorunScriptModel::ScriptInfo");
#endif
}

AutorunScriptModel::AutorunScriptModel(QObject* parent)
	: QAbstractListModel(parent)
{
	// Nothing to do
}

void AutorunScriptModel::deserialize(const QList<QVariant>& autorun) {
	for (const auto& item : autorun) {
		if (!item.canConvert<ScriptInfo>()) {
			continue;
		}
		ScriptInfo info = qvariant_cast<ScriptInfo>(item);
		if (info.filename.isEmpty()) {
			continue;
		}
		m_scripts.append(qvariant_cast<ScriptInfo>(item));
	}
	emitScriptsChanged();
}

int AutorunScriptModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_scripts.count();
}

bool AutorunScriptModel::setData(const QModelIndex& index, const QVariant& data, int role) {
	if (!index.isValid() || index.parent().isValid() || index.row() >= m_scripts.count()) {
		return false;
	}

	switch (role) {
	case Qt::CheckStateRole:
		m_scripts[index.row()].active = data.value<Qt::CheckState>() == Qt::Checked;
		emit dataChanged(index, index);
		emitScriptsChanged();
		return true;
	}
	return false;

}

QVariant AutorunScriptModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid() || index.parent().isValid() || index.row() >= m_scripts.count()) {
		return {};
	}

	switch (role) {
	case Qt::DisplayRole:
		return m_scripts.at(index.row()).filename;
	case Qt::CheckStateRole:
		return m_scripts.at(index.row()).active ? Qt::Checked : Qt::Unchecked;
	}
	return {};
}

Qt::ItemFlags AutorunScriptModel::flags(const QModelIndex& index) const {
	if (!index.isValid() || index.parent().isValid()) {
		return Qt::ItemIsDropEnabled;
	}
	return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren | Qt::ItemIsDragEnabled;
}

Qt::DropActions AutorunScriptModel::supportedDropActions() const {
	return Qt::CopyAction | Qt::MoveAction;
}

bool AutorunScriptModel::removeRows(int row, int count, const QModelIndex& parent) {
	if (parent.isValid()) {
		return false;
	}
	int lastRow = row + count - 1;
	if (row < 0 || lastRow >= m_scripts.size() || count < 0) {
		return false;
	}
	beginRemoveRows(QModelIndex(), row, lastRow);
	m_scripts.erase(m_scripts.begin() + row, m_scripts.begin() + row + count);
	endRemoveRows();
	emitScriptsChanged();
	return true;
}

bool AutorunScriptModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild) {
	if (sourceParent.isValid() || destinationParent.isValid()) {
		return false;
	}

	if (sourceRow < 0 || destinationChild < 0 || count <= 0) {
		return false;
	}

	int lastSource = sourceRow + count - 1;

	if (lastSource >= m_scripts.size() || destinationChild > m_scripts.size()) {
		return false;
	}

	if (sourceRow == destinationChild - 1) {
		return false;
	}

	bool ok = beginMoveRows(QModelIndex(), sourceRow, lastSource, QModelIndex(), destinationChild);
	if (!ok) {
		return false;
	}
	if (destinationChild < sourceRow) {
		sourceRow = lastSource;
	} else {
		destinationChild -= 1;
	}
	for (int i = 0; i < count; i++) {
		m_scripts.move(sourceRow, destinationChild);
	}
	endMoveRows();
	emitScriptsChanged();
	return true;
}

void AutorunScriptModel::addScript(const QString& filename) {
	beginInsertRows({}, m_scripts.count(), m_scripts.count());
	m_scripts.append(ScriptInfo { filename, true });
	endInsertRows();
	emitScriptsChanged();
}

QStringList AutorunScriptModel::activeScripts() const {
	QStringList scripts;
	for (const auto& pair: m_scripts) {
		if (!pair.active) {
			continue;
		}
		scripts.append(pair.filename);
	}
	return scripts;
}

QList<QVariant> AutorunScriptModel::serialize() const {
	QList<QVariant> list;
	for (const auto& script : m_scripts) {
		list.append(QVariant::fromValue(script));
	}
	return list;
}

void AutorunScriptModel::emitScriptsChanged() {
	emit scriptsChanged(serialize());
}
