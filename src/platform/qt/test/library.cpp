/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "platform/qt/library/LibraryModel.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
#include <QAbstractItemModelTester>
#endif

#include <QSignalSpy>
#include <QTest>

#define FIND_GBA_ROW(gba, gb) \
	int gba = findGBARow(); \
	if (gba < 0) QFAIL("Could not find gba row"); \
	int gb = 1 - gba;

using namespace QGBA;

class LibraryModelTest : public QObject {
Q_OBJECT

private:
	LibraryModel* model = nullptr;

	int findGBARow() {
		for (int i = 0; i < model->rowCount(); i++) {
			if (model->index(i, 0).data() == "gba") {
				return i;
			}
		}
		return -1;
	}

	LibraryEntry makeGBA(const QString& name, uint32_t crc) {
		LibraryEntry entry;
		entry.base = "/gba";
		entry.filename = name + ".gba";
		entry.fullpath = entry.base + "/" + entry.filename;
		entry.title = name;
		entry.internalTitle = name.toUpper().toUtf8();
		entry.internalCode = entry.internalTitle.replace(" ", "").left(4);
		entry.platform = mPLATFORM_GBA;
		entry.filesize = entry.fullpath.size() * 4;
		entry.crc32 = crc;
		return entry;
	}

	LibraryEntry makeGB(const QString& name, uint32_t crc) {
		LibraryEntry entry = makeGBA(name, crc);
		entry.base = "/gb";
		entry.filename = entry.filename.replace("gba", "gb");
		entry.fullpath = entry.fullpath.replace("gba", "gb");
		entry.platform = mPLATFORM_GB;
		entry.filesize /= 4;
		return entry;
	}

	void addTestGames1() {
		model->addEntries({
			makeGBA("Test Game", 0x12345678),
			makeGBA("Another", 0x23456789),
			makeGB("Old Game", 0x87654321),
		});
	}

	void addTestGames2() {
		model->addEntries({
			makeGBA("Game 3", 0x12345679),
			makeGBA("Game 4", 0x2345678A),
			makeGBA("Game 5", 0x2345678B),
			makeGB("Game 6", 0x87654322),
			makeGB("Game 7", 0x87654323),
		});
	}

	void updateGame() {
		LibraryEntry game = makeGBA("Another", 0x88888888);
		model->updateEntries({ game });
		QModelIndex idx = find("Another");
		QVERIFY2(idx.isValid(), "game not found");
		QCOMPARE(idx.siblingAtColumn(LibraryModel::COL_CRC32).data(Qt::EditRole).toInt(), 0x88888888);
	}

	void removeGames1() {
		model->removeEntries({ "/gba/Another.gba", "/gb/Game 6.gb" });
		QVERIFY2(!find("Another").isValid(), "game not removed");
		QVERIFY2(!find("Game 6").isValid(), "game not removed");
	}

	void removeGames2() {
		model->removeEntries({ "/gb/Old Game.gb", "/gb/Game 7.gb" });
		QVERIFY2(!find("Old Game").isValid(), "game not removed");
		QVERIFY2(!find("Game 7").isValid(), "game not removed");
	}

	QModelIndex find(const QString& name) {
		for (int i = 0; i < model->rowCount(); i++) {
			QModelIndex idx = model->index(i, 0);
			if (idx.data().toString() == name) {
				return idx;
			}
			for (int j = 0; j < model->rowCount(idx); j++) {
				QModelIndex child = model->index(j, 0, idx);
				if (child.data().toString() == name) {
					return child;
				}
			}
		}
		return QModelIndex();
	}

private slots:
	void init() {
		model = new LibraryModel();
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
		new QAbstractItemModelTester(model, QAbstractItemModelTester::FailureReportingMode::QtTest, model);
#endif
	}

	void cleanup() {
		delete model;
		model = nullptr;
	}

	void testList() {
		addTestGames1();
		QCOMPARE(model->rowCount(), 3);
		QCOMPARE(model->m_games.size(), 3);
		addTestGames2();
		QCOMPARE(model->rowCount(), 8);
		QCOMPARE(model->m_games.size(), 8);
		updateGame();
		QCOMPARE(model->m_games.size(), 8);
		model->removeEntries({ "/gba/Another.gba", "/gb/Game 6.gb" });
		QCOMPARE(model->rowCount(), 6);
		QCOMPARE(model->m_games.size(), 6);
		model->removeEntries({ "/gb/Old Game.gb", "/gb/Game 7.gb" });
		QCOMPARE(model->rowCount(), 4);
		QCOMPARE(model->m_games.size(), 4);
	}

	void testTree() {
		model->setTreeMode(true);
		addTestGames1();
		FIND_GBA_ROW(gbaRow, gbRow);
		QCOMPARE(model->rowCount(), 2);
		QCOMPARE(model->rowCount(model->index(gbRow, 0)), 1);
		QCOMPARE(model->rowCount(model->index(gbaRow, 0)), 2);
		QCOMPARE(model->m_games.size(), 3);
		addTestGames2();
		QCOMPARE(model->rowCount(), 2);
		QCOMPARE(model->rowCount(model->index(gbRow, 0)), 3);
		QCOMPARE(model->rowCount(model->index(gbaRow, 0)), 5);
		QCOMPARE(model->m_games.size(), 8);
		updateGame();
		QCOMPARE(model->m_games.size(), 8);
		removeGames1();
		QCOMPARE(model->rowCount(), 2);
		QCOMPARE(model->rowCount(model->index(gbRow, 0)), 2);
		QCOMPARE(model->rowCount(model->index(gbaRow, 0)), 4);
		QCOMPARE(model->m_games.size(), 6);
		removeGames2();
		QVERIFY2(!find("gb").isValid(), "did not remove gb folder");
		QCOMPARE(model->rowCount(), 1);
		QCOMPARE(model->rowCount(model->index(0, 0)), 4);
		QCOMPARE(model->m_games.size(), 4);
	}

	void modeSwitchTest1() {
		addTestGames1();
		{
			QSignalSpy resetSpy(model, SIGNAL(modelReset()));
			model->setTreeMode(true);
			QVERIFY(resetSpy.count());
		}
		FIND_GBA_ROW(gbaRow, gbRow);
		QCOMPARE(model->rowCount(), 2);
		QCOMPARE(model->rowCount(model->index(gbRow, 0)), 1);
		QCOMPARE(model->rowCount(model->index(gbaRow, 0)), 2);
		{
			QSignalSpy resetSpy(model, SIGNAL(modelReset()));
			model->setTreeMode(false);
			QVERIFY(resetSpy.count());
		}
		addTestGames2();
		QCOMPARE(model->rowCount(), 8);
	}

	void modeSwitchTest2() {
		model->setTreeMode(false);
		addTestGames1();
		model->setTreeMode(true);
		FIND_GBA_ROW(gbaRow, gbRow);
		QCOMPARE(model->rowCount(), 2);
		QCOMPARE(model->rowCount(model->index(gbRow, 0)), 1);
		QCOMPARE(model->rowCount(model->index(gbaRow, 0)), 2);
		addTestGames2();
		QCOMPARE(model->rowCount(), 2);
		QCOMPARE(model->rowCount(model->index(gbRow, 0)), 3);
		QCOMPARE(model->rowCount(model->index(gbaRow, 0)), 5);
		model->setTreeMode(false);
		QCOMPARE(model->rowCount(), 8);
	}
};

QTEST_MAIN(LibraryModelTest)
#include "library.moc"
