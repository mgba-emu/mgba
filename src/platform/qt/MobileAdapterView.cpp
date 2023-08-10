/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MobileAdapterView.h"

#include "ConfigController.h"
#include "CoreController.h"
#include "GBAApp.h"
#include "ShortcutController.h"
#include "Window.h"

#include <QtAlgorithms>
#include <QClipboard>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMessageBox>
#include <QMultiMap>
#include <QSettings>
#include <QStringList>

using namespace QGBA;

MobileAdapterView::MobileAdapterView(std::shared_ptr<CoreController> controller, Window* window, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
	, m_window(window)
{
	m_ui.setupUi(this);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	int size = QFontMetrics(QFont()).height() / ((int) ceil(devicePixelRatioF()) * 12);
#else
	int size = QFontMetrics(QFont()).height() / (devicePixelRatio() * 12);
#endif
	if (!size) {
		size = 1;
	}

	m_ui.setDns1->setInputMask("009.009.009.009;_");
	m_ui.setDns2->setInputMask("009.009.009.009;_");
	m_ui.setRelay->setInputMask("009.009.009.009;_");

	QRegularExpression re("[\\dA-Fa-f]{32}?");
	QRegularExpressionValidator v(re, m_ui.setRelay);
	m_ui.setRelay->setValidator(&v);

	connect(m_ui.setType, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &MobileAdapterView::setType);
	connect(m_ui.setUnmetered, &QAbstractButton::toggled, this, &MobileAdapterView::setUnmetered);
	connect(m_ui.setDns1, &QLineEdit::textChanged, this, &MobileAdapterView::setDns1);
	connect(m_ui.setDns2, &QLineEdit::textChanged, this, &MobileAdapterView::setDns2);
	connect(m_ui.setPort, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &MobileAdapterView::setPort);
	connect(m_ui.setRelay, &QLineEdit::textChanged, this, &MobileAdapterView::setRelay);
	connect(m_ui.setToken, &QLineEdit::textChanged, this, &MobileAdapterView::setToken);
	connect(m_ui.copyToken, &QAbstractButton::clicked, this, &MobileAdapterView::copyToken);

	connect(m_controller.get(), &CoreController::frameAvailable, this, &MobileAdapterView::advanceFrameCounter);
	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);

	m_controller->attachMobileAdapter();
	getConfig();
}

MobileAdapterView::~MobileAdapterView() {
	m_controller->detachMobileAdapter();
}

void MobileAdapterView::setType(int type) {
	m_controller->setMobileAdapterType(type);
}

void MobileAdapterView::setUnmetered(bool unmetered) {
	m_controller->setMobileAdapterUnmetered(unmetered);
}

void MobileAdapterView::setDns1(const QString& dns1) {
	m_controller->setMobileAdapterDns1(dns1, 53);
}

void MobileAdapterView::setDns2(const QString& dns2) {
	m_controller->setMobileAdapterDns2(dns2, 53);
}

void MobileAdapterView::setPort(int port) {
	m_controller->setMobileAdapterPort(port);
}

void MobileAdapterView::setRelay(const QString& relay) {
	m_controller->setMobileAdapterRelay(relay, 31227);
}

void MobileAdapterView::setToken(const QString& token) {
	if (m_controller->setMobileAdapterToken(token)) {
		m_ui.setToken->setText(token.simplified());
	} else {
		m_ui.setToken->setText("");
	}
}

void MobileAdapterView::copyToken(bool checked) {
	UNUSED(checked);
	QGuiApplication::clipboard()->setText(m_ui.setToken->text());
}

void MobileAdapterView::getConfig() {
	int type;
	bool unmetered;
	QString dns1, dns2;
	int port;
	QString relay;
	m_controller->getMobileAdapterConfig(&type, &unmetered, &dns1, &dns2, &port, &relay);
	m_ui.setType->setCurrentIndex(type);
	m_ui.setUnmetered->setChecked(unmetered);
	m_ui.setDns1->setText(dns1);
	m_ui.setDns2->setText(dns2);
	m_ui.setPort->setValue(port);
	m_ui.setRelay->setText(relay);
	advanceFrameCounter();
}

void MobileAdapterView::advanceFrameCounter() {
	QString userNumber, peerNumber, token;
	m_controller->updateMobileAdapter(&userNumber, &peerNumber, &token);
	m_ui.userNumber->setText(userNumber);
	m_ui.peerNumber->setText(peerNumber);
	m_ui.setToken->setText(token);
}
