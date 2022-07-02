/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ScriptingTextBufferModel.h"

#include "ScriptingTextBuffer.h"

#include <QTextDocument>

using namespace QGBA;

ScriptingTextBufferModel::ScriptingTextBufferModel(QObject* parent)
	: QAbstractListModel(parent)
{
	// initializers only
}

void ScriptingTextBufferModel::attachToContext(mScriptContext* context)
{
	mScriptContextSetTextBufferFactory(context, &ScriptingTextBufferModel::createTextBuffer, this);
}

void ScriptingTextBufferModel::reset() {
	beginResetModel();
	QList<ScriptingTextBuffer*> toDelete = m_buffers;
	m_buffers.clear();
	endResetModel();
	for (ScriptingTextBuffer* buffer : toDelete) {
		delete buffer;
	}
}

mScriptTextBuffer* ScriptingTextBufferModel::createTextBuffer(void* context) {
	ScriptingTextBufferModel* self = static_cast<ScriptingTextBufferModel*>(context);
	self->beginInsertRows(QModelIndex(), self->m_buffers.size(), self->m_buffers.size() + 1);
	ScriptingTextBuffer* buffer = new ScriptingTextBuffer;
	if (buffer->thread() != self->thread()) {
		buffer->moveToThread(self->thread());
	}
	buffer->setParent(self);
	QObject::connect(buffer, &ScriptingTextBuffer::bufferNameChanged, self, &ScriptingTextBufferModel::bufferNameChanged);
	self->m_buffers.append(buffer);
	emit self->textBufferCreated(buffer);
	self->endInsertRows();
	return buffer->textBuffer();
}

void ScriptingTextBufferModel::bufferNameChanged(const QString&) {
	ScriptingTextBuffer* buffer = qobject_cast<ScriptingTextBuffer*>(sender());
	int row = m_buffers.indexOf(buffer);
	if (row < 0) {
		return;
	}
	QModelIndex idx = index(row, 0);
	emit dataChanged(idx, idx, { Qt::DisplayRole });
}

int ScriptingTextBufferModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_buffers.size();
}

QVariant ScriptingTextBufferModel::data(const QModelIndex& index, int role) const {
	if (index.parent().isValid() || index.row() < 0 || index.row() >= m_buffers.size() || index.column() != 0) {
		return QVariant();
	}
	if (role == Qt::DisplayRole) {
		return m_buffers[index.row()]->document()->metaInformation(QTextDocument::DocumentTitle);
	} else if (role == ScriptingTextBufferModel::DocumentRole) {
		return QVariant::fromValue<QTextDocument*>(m_buffers[index.row()]->document());
	}
	return QVariant();
}
