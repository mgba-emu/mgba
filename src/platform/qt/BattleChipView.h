/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "BattleChipModel.h"

#include <QDialog>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QHash>

#include <memory>

#include <mgba/core/interface.h>

#include "ui_BattleChipView.h"

class QTcpServer;
class QTcpSocket;

namespace QGBA {

class CoreController;
class Window;

class BattleChipView : public QDialog {
Q_OBJECT

public:
	BattleChipView(std::shared_ptr<CoreController> controller, Window* window, QWidget* parent = nullptr);
	~BattleChipView();

public slots:
	void setFlavor(int);
	void insertChip(bool);
	void reinsert();
	void resort();

private slots:
	void advanceFrameCounter();
	void addChip();
	void removeChip();

	void saveDeck();
	void loadDeck();

	void updateData();

	void onChipIdChanged(int id);

	void netGateToggled(bool on);
	void netGatePortChanged(const QString& s);
	void netGateBindChanged(const QString& s);
	void netGateNewConnection();
	void netGateReadyRead(QTcpSocket* sock);

private:
	static const int UNINSERTED_TIME = 10;

	void loadChipNames(int);

	Ui::BattleChipView m_ui;

	BattleChipModel m_model;
	std::shared_ptr<CoreController> m_controller;

	int m_frameCounter = -1;
	bool m_next = false;

	Window* m_window;

	BattleChipUpdater* m_updater = nullptr;

	QTcpServer* m_netGateServer = nullptr;
	QList<QTcpSocket*> m_netClients;
	QHash<QTcpSocket*, QByteArray> m_netBufs;
	quint16 m_netPort;
	QString m_netBindStr;

	void netGateStart();
	void netGateStop();
	void netGateSetStatus(const QString& text);
	void netGateDisableFields(bool disable);
	void netGateApplyChip(quint16 chipId);
};

}
