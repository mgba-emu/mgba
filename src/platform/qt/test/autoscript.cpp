/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "platform/qt/scripting/AutorunScriptModel.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#include <QAbstractItemModelTester>
#endif

#include <QDataStream>
#include <QSignalSpy>
#include <QTest>

using namespace QGBA;

class AutorunScriptModelTest : public QObject {
Q_OBJECT

private:
	AutorunScriptModel* model = nullptr;
	QSignalSpy* spy = nullptr;

	void addEntries(bool deactivateSecond) {
		model->addScript("foo");
		model->addScript("bar");
		model->addScript("baz");
		QCOMPARE(spy->count(), 3);
		if (deactivateSecond) {
			bool ok = model->setData(model->index(1, 0), Qt::Unchecked, Qt::CheckStateRole);
			QCOMPARE(ok, true);
		}
	}

	void checkScripts(const QStringList& names) {
		int count = names.size();
		for (int i = 0; i < count; i++) {
			QCOMPARE(model->data(model->index(i, 0)), names[i]);
		}
	}

	QVariantList parseRawData(const char* source, int size) {
		QByteArray rawData(source, size);
		QVariantList data;
		QDataStream ds(&rawData, QIODevice::ReadOnly);
		ds >> data;
		return data;
	}

private slots:
	void initTestCase() {
		AutorunScriptModel::registerMetaTypes();
	}

	void init() {
		model = new AutorunScriptModel(nullptr);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		new QAbstractItemModelTester(model, QAbstractItemModelTester::FailureReportingMode::QtTest, model);
#endif
		spy = new QSignalSpy(model, SIGNAL(scriptsChanged(QList<QVariant>)));
	}

	void cleanup() {
		delete model;
		model = nullptr;
		delete spy;
		spy = nullptr;
	}

	void testAdd() {
		addEntries(false);
		QCOMPARE(model->rowCount(), 3);
	}

	void testEdit() {
		addEntries(true);
		int before = spy->count();
		bool ok = model->setData(model->index(0, 0), Qt::Unchecked, Qt::CheckStateRole);
		QCOMPARE(ok, true);
		QCOMPARE(model->data(model->index(0, 0), Qt::CheckStateRole), Qt::Unchecked);
		QCOMPARE(model->activeScripts(), (QStringList{ "baz" }));
		QCOMPARE(spy->count() - before, 1);
	}

	void testMoveUp() {
		addEntries(true);
		int before = spy->count();
		bool ok = model->moveRow(QModelIndex(), 1, QModelIndex(), 0);
		QCOMPARE(ok, true);
		checkScripts({ "bar", "foo", "baz" });
		QCOMPARE(model->data(model->index(0, 0), Qt::CheckStateRole), Qt::Unchecked);
		QCOMPARE(spy->count() - before, 1);
	}

	void testMoveDown() {
		addEntries(true);
		int before = spy->count();
		bool ok = model->moveRow(QModelIndex(), 1, QModelIndex(), 3);
		QCOMPARE(ok, true);
		checkScripts({ "foo", "baz", "bar" });
		QCOMPARE(model->data(model->index(2, 0), Qt::CheckStateRole), Qt::Unchecked);
		QCOMPARE(spy->count() - before, 1);
	}

	void testRemove() {
		addEntries(false);
		int before = spy->count();
		bool ok = model->removeRow(1);
		QCOMPARE(ok, true);
		checkScripts({ "foo", "baz" });
		QCOMPARE(model->activeScripts(), (QStringList{ "foo", "baz" }));
		QCOMPARE(spy->count() - before, 1);
	}

	void testSerialize() {
		addEntries(true);
		int before = spy->count();
		auto data = model->serialize();
		QCOMPARE(data.size(), 3);
		auto first = data.first().value<AutorunScriptModel::ScriptInfo>();
		QCOMPARE(first.filename, "foo");
		QCOMPARE(first.active, true);
		auto second = data[1].value<AutorunScriptModel::ScriptInfo>();
		QCOMPARE(second.filename, "bar");
		QCOMPARE(second.active, false);
		QCOMPARE(spy->count() - before, 0);
	}

	void testDeserialize() {
		QVariantList data;
		data << QVariant::fromValue(AutorunScriptModel::ScriptInfo{ "foo", true });
		data << QVariant::fromValue(AutorunScriptModel::ScriptInfo{ "bar", false });
		QByteArray rawData;
		QDataStream ds(&rawData, QIODevice::WriteOnly);
		ds << data;
		model->deserialize(data);
		QCOMPARE(model->rowCount(), 2);
		checkScripts({ "foo", "bar" });
		QCOMPARE(model->data(model->index(0, 0), Qt::CheckStateRole), Qt::Checked);
		QCOMPARE(model->data(model->index(1, 0), Qt::CheckStateRole), Qt::Unchecked);
		QCOMPARE(spy->count(), 1);
	}

	void testDeserializeInvalid() {
		static const char v0Data[] =
			"\0\0\0\1"
			"\0\0\4\0\0\0\0\0%QGBA::AutorunScriptModel::ScriptInfo\0\0\0\0\0\3foo\1";
		model->deserialize(parseRawData(v0Data, sizeof(v0Data)));
		QCOMPARE(model->rowCount(), 0);
	}

	void testDeserializeV1() {
		static const char v1Data[] =
			"\0\0\0\2"
			"\0\0\4\0\0\0\0\0%QGBA::AutorunScriptModel::ScriptInfo\0\0\1\0\0\0\3foo\1"
			"\0\0\4\0\0\0\0\0%QGBA::AutorunScriptModel::ScriptInfo\0\0\1\0\0\0\3bar\0";
		model->deserialize(parseRawData(v1Data, sizeof(v1Data)));
		QCOMPARE(model->rowCount(), 2);
		checkScripts({ "foo", "bar" });
		QCOMPARE(model->data(model->index(0, 0), Qt::CheckStateRole), Qt::Checked);
		QCOMPARE(model->data(model->index(1, 0), Qt::CheckStateRole), Qt::Unchecked);
	}
};

QTEST_MAIN(AutorunScriptModelTest)
#include "autoscript.moc"
