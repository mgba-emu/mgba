/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "BattleChipView.h"

#include "BattleChipUpdater.h"
#include "ConfigController.h"
#include "CoreController.h"
#include "GBAApp.h"
#include "ShortcutController.h"
#include "Window.h"
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/memory.h>

#include <QtAlgorithms>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMessageBox>
#include <QMultiMap>
#include <QSettings>
#include <QStringList>

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

using namespace QGBA;

BattleChipView::BattleChipView(std::shared_ptr<CoreController> controller, Window* window, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(std::move(controller))
	, m_window(window)
{
	m_ui.setupUi(this);
	m_ui.chipList->setModel(&m_model);

	CoreController::Interrupter interrupter(m_controller);
	mCore* core = m_controller->thread()->core;
	mGameInfo info;
	QString qtitle;

	if (core->platform(core) == mPLATFORM_GBA) {
		struct GBA* gba = (struct GBA*) core->board;
		char code[5];
		code[0] = (char) GBAView8(gba->cpu, 0x080000AC);
		code[1] = (char) GBAView8(gba->cpu, 0x080000AD);
		code[2] = (char) GBAView8(gba->cpu, 0x080000AE);
		code[3] = (char) GBAView8(gba->cpu, 0x080000AF);
		code[4] = '\0';
		qtitle = QString::fromLatin1(code);
	} else {
		core->getGameInfo(core, &info);
		qtitle = QString::fromLatin1(info.title).right(4);
	}

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	int size = QFontMetrics(QFont()).height() / ((int) ceil(devicePixelRatioF()) * 12);
#else
	int size = QFontMetrics(QFont()).height() / (devicePixelRatio() * 12);
#endif
	if (!size) {
		size = 1;
	}
	m_ui.chipList->setIconSize(m_ui.chipList->iconSize() * size);
	m_ui.chipList->setGridSize(m_ui.chipList->gridSize() * size);
	m_model.setScale(size);

	connect(m_ui.chipName, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int idx) {
		if (idx < 0) return;
		const auto keys = m_model.chipNames().keys();
		if (idx >= 0 && idx < keys.size()) {
			bool blocked = m_ui.chipId->blockSignals(true);
			m_ui.chipId->setValue(keys[idx]);
			m_ui.chipId->blockSignals(blocked);
		}
	});

	connect(m_ui.inserted, &QAbstractButton::toggled, this, &BattleChipView::insertChip);
	connect(m_ui.chipId,  static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &BattleChipView::onChipIdChanged);
	connect(m_ui.insert,  &QAbstractButton::clicked, this, &BattleChipView::reinsert);
	connect(m_ui.add,     &QAbstractButton::clicked, this, &BattleChipView::addChip);
	connect(m_ui.remove,  &QAbstractButton::clicked, this, &BattleChipView::removeChip);
	connect(m_ui.save,    &QAbstractButton::clicked, this, &BattleChipView::saveDeck);
	connect(m_ui.load,    &QAbstractButton::clicked, this, &BattleChipView::loadDeck);
	connect(m_ui.updateData, &QAbstractButton::clicked, this, &BattleChipView::updateData);
	connect(m_ui.buttonBox->button(QDialogButtonBox::Reset), &QAbstractButton::clicked, &m_model, &BattleChipModel::clear);

	connect(m_ui.gateBattleChip, &QAbstractButton::toggled, this, [this](bool on) {
		if (on) {
			setFlavor(GBA_FLAVOR_BATTLECHIP_GATE);
		}
	});
	connect(m_ui.gateProgress, &QAbstractButton::toggled, this, [this](bool on) {
		if (on) {
			setFlavor(GBA_FLAVOR_PROGRESS_GATE);
		}
	});
	connect(m_ui.gateBeastLink, &QAbstractButton::toggled, this, [this, qtitle](bool on) {
		if (on) {
			if (qtitle.endsWith('E') || qtitle.endsWith('P')) {
				setFlavor(GBA_FLAVOR_BEAST_LINK_GATE_US);
			} else {
				setFlavor(GBA_FLAVOR_BEAST_LINK_GATE);
			}
		}
	});

	connect(m_controller.get(), &CoreController::frameAvailable, this, &BattleChipView::advanceFrameCounter);

	connect(m_ui.chipList, &QAbstractItemView::clicked, this, [this](const QModelIndex& index) {
		QVariant chip = m_model.data(index, Qt::UserRole);
		bool blocked = m_ui.chipId->blockSignals(true);
		m_ui.chipId->setValue(chip.toInt());
		m_ui.chipId->blockSignals(blocked);
		reinsert();
	});
	connect(m_ui.chipList, &QListView::indexesMoved, this, &BattleChipView::resort);

	m_controller->attachBattleChipGate();
	setFlavor(4);
	if (qtitle.startsWith("B4B") || qtitle.startsWith("B4W") || qtitle.startsWith("BR4") || qtitle.startsWith("BZ3")) {
		m_ui.gateBattleChip->setChecked(true);
	} else if (qtitle.startsWith("BRB") || qtitle.startsWith("BRK")) {
		m_ui.gateProgress->setChecked(true);
	} else if (qtitle.startsWith("BR5") || qtitle.startsWith("BR6")) {
		m_ui.gateBeastLink->setChecked(true);
	}

	if (!QFileInfo(GBAApp::dataDir() + "/chips.rcc").exists() && !QFileInfo(ConfigController::configDir() + "/chips.rcc").exists()) {
		QMessageBox* download = new QMessageBox(this);
		download->setIcon(QMessageBox::Information);
		download->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		download->setWindowTitle(tr("BattleChip data missing"));
		download->setText(tr("BattleChip data is missing. BattleChip Gates will still work, but some graphics will be missing. Would you like to download the data now?"));
		download->setAttribute(Qt::WA_DeleteOnClose);
		download->setWindowModality(Qt::NonModal);
		connect(download, &QDialog::accepted, this, &BattleChipView::updateData);
		download->show();
	}

	if (m_ui.netGateEnable && m_ui.netGatePort && m_ui.netGateBind && m_ui.netGateStatus) {
		connect(m_ui.netGateEnable, &QCheckBox::toggled, this, &BattleChipView::netGateToggled);
		connect(m_ui.netGatePort,   &QLineEdit::textChanged, this, &BattleChipView::netGatePortChanged);
		connect(m_ui.netGateBind,   &QLineEdit::textChanged, this, &BattleChipView::netGateBindChanged);

		bool ok = false;
		quint32 p = m_ui.netGatePort->text().toUInt(&ok);
		if (ok && p > 0 && p <= 65535) {
			m_netPort = static_cast<quint16>(p);
		}
		m_netBindStr = m_ui.netGateBind->text().trimmed();
	}
}

BattleChipView::~BattleChipView() {
	netGateStop();
	m_controller->detachBattleChipGate();
}

void BattleChipView::setFlavor(int flavor) {
	m_controller->setBattleChipFlavor(flavor);
	m_model.setFlavor(flavor);
	m_ui.chipName->clear();
	m_ui.chipName->addItems(m_model.chipNames().values());
}

void BattleChipView::insertChip(bool inserted) {
	if (inserted) {
		m_controller->setBattleChipId(m_ui.chipId->value());
	} else {
		m_controller->setBattleChipId(0);
	}
}

void BattleChipView::reinsert() {
	const bool keepInserted = m_ui.inserted->isChecked();
	if (keepInserted) {
		insertChip(false);
		m_next = true;
		m_frameCounter = UNINSERTED_TIME;
	} else {
		insertChip(true);
		m_next = false;
		m_frameCounter = UNINSERTED_TIME;
	}
	m_window->setWindowState(m_window->windowState() & ~Qt::WindowActive);
	m_window->setWindowState(m_window->windowState() | Qt::WindowActive);
}

void BattleChipView::addChip() {
	int insertedChip = m_ui.chipId->value();
	if (insertedChip < 1) {
		return;
	}
	m_model.addChip(insertedChip);
}

void BattleChipView::removeChip() {
	for (const auto& index : m_ui.chipList->selectionModel()->selectedIndexes()) {
		m_model.removeChip(index);
	}
}

void BattleChipView::advanceFrameCounter() {
	if (m_frameCounter == 0) {
		insertChip(m_next);
	}
	if (m_frameCounter >= 0) {
		--m_frameCounter;
	}
}

void BattleChipView::saveDeck() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Select deck file"), tr(("BattleChip deck file (*.deck)")));
	if (filename.isEmpty()) {
		return;
	}

	QStringList deck;
	for (int i = 0; i < m_model.rowCount(); ++i) {
		deck.append(m_model.data(m_model.index(i, 0), Qt::UserRole).toString());
	}

	QSettings ini(filename, QSettings::IniFormat);
	ini.clear();
	ini.beginGroup("BattleChipDeck");
	ini.setValue("version", m_model.flavor());
	ini.setValue("deck", deck.join(','));
	ini.sync();
}

void BattleChipView::loadDeck() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select deck file"), tr(("BattleChip deck file (*.deck)")));
	if (filename.isEmpty()) {
		return;
	}

	QSettings ini(filename, QSettings::IniFormat);
	ini.beginGroup("BattleChipDeck");
	int flavor = ini.value("version").toInt();
	if (flavor != m_model.flavor()) {
		QMessageBox* error = new QMessageBox(this);
		error->setIcon(QMessageBox::Warning);
		error->setStandardButtons(QMessageBox::Ok);
		error->setWindowTitle(tr("Incompatible deck"));
		error->setText(tr("The selected deck is not compatible with this Chip Gate"));
		error->setAttribute(Qt::WA_DeleteOnClose);
		error->show();
		return;
	}

	QList<int> newDeck;
	QStringList deck = ini.value("deck").toString().split(',');
	for (const auto& item : deck) {
		bool ok;
		int id = item.toInt(&ok);
		if (ok) {
			newDeck.append(id);
		}
	}
	m_model.setChips(newDeck);
}

void BattleChipView::resort() {
	QMap<int, int> chips;
	for (int i = 0; i < m_model.rowCount(); ++i) {
		QModelIndex index = m_model.index(i, 0);
		QRect visualRect = m_ui.chipList->visualRect(index);
		QSize gridSize = m_ui.chipList->gridSize();
		int x = visualRect.y() / gridSize.height();
		x *= m_ui.chipList->viewport()->width();
		x += visualRect.x();
		x *= m_model.rowCount();
		x += index.row();
		chips[x] = m_model.data(index, Qt::UserRole).toInt();
	}
	m_model.setChips(chips.values());
}

void BattleChipView::updateData() {
	if (m_updater) {
		return;
	}
	m_updater = new BattleChipUpdater(this);
	connect(m_updater, &BattleChipUpdater::updateDone, this, [this](bool success) {
		if (success) {
			m_model.reloadAssets();
			m_ui.chipName->clear();
			m_ui.chipName->addItems(m_model.chipNames().values());
		}
		delete m_updater;
		m_updater = nullptr;
	});
	m_updater->downloadUpdate();
}

void BattleChipView::netGateToggled(bool on) {
	if (on) {
		netGateStart();
	} else {
		netGateStop();
	}
}

void BattleChipView::netGatePortChanged(const QString& s) {
	bool ok = false;
	quint32 p = s.toUInt(&ok);
	if (ok && p > 0 && p <= 65535) {
		m_netPort = static_cast<quint16>(p);
		if (m_netGateServer && m_netGateServer->isListening()) {
			netGateStop();
			netGateStart();
		}
	}
}

void BattleChipView::netGateBindChanged(const QString& s) {
	m_netBindStr = s.trimmed();
	if (m_netGateServer && m_netGateServer->isListening()) {
		netGateStop();
		netGateStart();
	}
}

void BattleChipView::netGateStart() {
	if (!m_ui.netGateEnable || !m_ui.netGatePort || !m_ui.netGateBind || !m_ui.netGateStatus) {
		return;
	}

	if (!m_netPort) {
		netGateSetStatus(tr("Failed: invalid port"));
		m_ui.netGateEnable->setChecked(false);
		return;
	}
	if (!m_netGateServer) {
		m_netGateServer = new QTcpServer(this);
		connect(m_netGateServer, &QTcpServer::newConnection, this, &BattleChipView::netGateNewConnection);
	}
	QHostAddress bindAddr;
	if (!bindAddr.setAddress(m_netBindStr)) {
		netGateSetStatus(tr("Failed: invalid bind address"));
		m_ui.netGateEnable->setChecked(false);
		return;
	}
	if (!m_netGateServer->listen(bindAddr, m_netPort)) {
		netGateSetStatus(tr("Failed: %1").arg(m_netGateServer->errorString()));
		m_ui.netGateEnable->setChecked(false);
		return;
	}
	netGateDisableFields(true);
	netGateSetStatus(tr("Listening on %1:%2").arg(bindAddr.toString()).arg(m_netPort));
}

void BattleChipView::netGateStop() {
	for (QTcpSocket* c : m_netClients) {
		if (!c) continue;
		c->disconnect(this);
		c->close();
		c->deleteLater();
	}
	m_netClients.clear();
	m_netBufs.clear();

	if (m_netGateServer) {
		m_netGateServer->close();
	}

	netGateDisableFields(false);
	netGateSetStatus(tr("Stopped"));
}

void BattleChipView::netGateNewConnection() {
	if (!m_netGateServer) return;
	while (m_netGateServer->hasPendingConnections()) {
		QTcpSocket* c = m_netGateServer->nextPendingConnection();
		m_netClients.append(c);
		m_netBufs.insert(c, QByteArray());
		connect(c, &QTcpSocket::readyRead, this, [this, c]() { netGateReadyRead(c); });
		connect(c, &QTcpSocket::disconnected, this, [this, c]() {
			m_netBufs.remove(c);
			m_netClients.removeAll(c);
			c->deleteLater();
		});
	}
}

void BattleChipView::netGateReadyRead(QTcpSocket* sock) {
	if (!sock) return;
	QByteArray& buf = m_netBufs[sock];
	buf += sock->readAll();

	for (;;) {
		int hdr = buf.indexOf(char(0x80));
		if (hdr < 0) { buf.clear(); return; }
		if (buf.size() - hdr < 3) {
			if (hdr > 0) buf.remove(0, hdr);
			return;
		}
		const unsigned char b0 = (unsigned char)buf.at(hdr + 0);
		const unsigned char b1 = (unsigned char)buf.at(hdr + 1);
		const unsigned char b2 = (unsigned char)buf.at(hdr + 2);
		buf.remove(0, hdr + 3);
		if (b0 != 0x80) continue;
		const quint16 chipId = (quint16(b1) << 8) | quint16(b2);

		netGateApplyChip(chipId);
	}
}

void BattleChipView::netGateApplyChip(quint16 chipId) {
	bool blocked = m_ui.chipId->blockSignals(true);
	m_ui.chipId->setValue(chipId);
	m_ui.chipId->blockSignals(blocked);
	onChipIdChanged(chipId);

	const bool keepInserted = m_ui.inserted->isChecked();
	if (keepInserted) {
		insertChip(false);
		m_next = true;
		m_frameCounter = UNINSERTED_TIME;
	} else {
		insertChip(true);
		m_next = false;
		m_frameCounter = UNINSERTED_TIME;
	}
}

void BattleChipView::netGateSetStatus(const QString& text) {
	if (m_ui.netGateStatus) {
		m_ui.netGateStatus->setText(text);
	}
}

void BattleChipView::netGateDisableFields(bool disable) {
	if (!m_ui.netGatePort || !m_ui.netGateBind) return;
	m_ui.netGatePort->setEnabled(!disable);
	m_ui.netGateBind->setEnabled(!disable);
}

void BattleChipView::onChipIdChanged(int id) {
	const auto names = m_model.chipNames();
	const QString name = names.value(id);
	if (!name.isEmpty()) {
		int idx = m_ui.chipName->findText(name);
		if (idx >= 0) {
			bool blocked = m_ui.chipName->blockSignals(true);
			m_ui.chipName->setCurrentIndex(idx);
			m_ui.chipName->blockSignals(blocked);
		}
	}
}
