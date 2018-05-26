/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LoadSaveState.h"

#include "CoreController.h"
#include "GamepadAxisEvent.h"
#include "GamepadButtonEvent.h"
#include "VFileDevice.h"

#include <QAction>
#include <QDateTime>
#include <QKeyEvent>
#include <QPainter>

#include <mgba/core/serialize.h>
#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

using namespace QGBA;

LoadSaveState::LoadSaveState(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
	, m_mode(LoadSave::LOAD)
	, m_currentFocus(controller->stateSlot() - 1)
{
	setAttribute(Qt::WA_TranslucentBackground);
	m_ui.setupUi(this);

	m_slots[0] = m_ui.state1;
	m_slots[1] = m_ui.state2;
	m_slots[2] = m_ui.state3;
	m_slots[3] = m_ui.state4;
	m_slots[4] = m_ui.state5;
	m_slots[5] = m_ui.state6;
	m_slots[6] = m_ui.state7;
	m_slots[7] = m_ui.state8;
	m_slots[8] = m_ui.state9;

	unsigned width, height;
	controller->thread()->core->desiredVideoDimensions(controller->thread()->core, &width, &height);
	int i;
	for (i = 0; i < NUM_SLOTS; ++i) {
		loadState(i + 1);
		m_slots[i]->installEventFilter(this);
		m_slots[i]->setMaximumSize(width + 2, height + 2);
		connect(m_slots[i], &QAbstractButton::clicked, this, [this, i]() { triggerState(i + 1); });
	}

	if (m_currentFocus >= 9) {
		m_currentFocus = 0;
	}
	if (m_currentFocus < 0) {
		m_currentFocus = 0;
	}

	QAction* escape = new QAction(this);
	connect(escape, &QAction::triggered, this, &QWidget::close);
	escape->setShortcut(QKeySequence("Esc"));
	escape->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(escape);

	connect(m_controller.get(), &CoreController::stopping, this, &QWidget::close);
}

void LoadSaveState::setMode(LoadSave mode) {
	m_mode = mode;
	QString text = mode == LoadSave::LOAD ? tr("Load State") : tr("Save State");
	setWindowTitle(text);
	m_ui.lsLabel->setText(text);
}

bool LoadSaveState::eventFilter(QObject* object, QEvent* event) {
	if (event->type() == QEvent::KeyPress) {
		int column = m_currentFocus % 3;
		int row = m_currentFocus / 3;
		switch (static_cast<QKeyEvent*>(event)->key()) {
		case Qt::Key_Up:
			row += 2;
			break;
		case Qt::Key_Down:
			row += 1;
			break;
		case Qt::Key_Left:
			column += 2;
			break;
		case Qt::Key_Right:
			column += 1;
			break;
		case Qt::Key_1:
		case Qt::Key_2:
		case Qt::Key_3:
		case Qt::Key_4:
		case Qt::Key_5:
		case Qt::Key_6:
		case Qt::Key_7:
		case Qt::Key_8:
		case Qt::Key_9:
			triggerState(static_cast<QKeyEvent*>(event)->key() - Qt::Key_1 + 1);
			break;
		case Qt::Key_Enter:
		case Qt::Key_Return:
			triggerState(m_currentFocus + 1);
			break;
		default:
			return false;
		}
		column %= 3;
		row %= 3;
		m_currentFocus = column + row * 3;
		m_slots[m_currentFocus]->setFocus();
		return true;
	}
	if (event->type() == QEvent::Enter) {
		int i;
		for (i = 0; i < 9; ++i) {
			if (m_slots[i] == object) {
				m_currentFocus = i;
				m_slots[m_currentFocus]->setFocus();
				return true;
			}
		}
	}
	if (event->type() == GamepadButtonEvent::Down() || event->type() == GamepadAxisEvent::Type()) {
		int column = m_currentFocus % 3;
		int row = m_currentFocus - column;
		GBAKey key = GBA_KEY_NONE;
		if (event->type() == GamepadButtonEvent::Down()) {
			key = static_cast<GamepadButtonEvent*>(event)->gbaKey();
		} else if (event->type() == GamepadAxisEvent::Type()) {
			GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
			if (gae->isNew()) {
				key = gae->gbaKey();
			} else {
				return false;
			}
		}
		switch (key) {
		case GBA_KEY_UP:
			row += 6;
			break;
		case GBA_KEY_DOWN:
			row += 3;
			break;
		case GBA_KEY_LEFT:
			column += 2;
			break;
		case GBA_KEY_RIGHT:
			column += 1;
			break;
		case GBA_KEY_B:
			event->accept();
			close();
			return true;
		case GBA_KEY_A:
		case GBA_KEY_START:
			event->accept();
			triggerState(m_currentFocus + 1);
			return true;
		default:
			return false;
		}
		column %= 3;
		row %= 9;
		m_currentFocus = column + row;
		m_slots[m_currentFocus]->setFocus();
		event->accept();
		return true;
	}
	return false;
}

void LoadSaveState::loadState(int slot) {
	mCoreThread* thread = m_controller->thread();
	VFile* vf = mCoreGetState(thread->core, slot, 0);
	if (!vf) {
		m_slots[slot - 1]->setText(tr("Empty"));
		return;
	}

	mStateExtdata extdata;
	mStateExtdataInit(&extdata);
	void* state = mCoreExtractState(thread->core, vf, &extdata);
	vf->seek(vf, 0, SEEK_SET);
	if (!state) {
		m_slots[slot - 1]->setText(tr("Corrupted"));
		mStateExtdataDeinit(&extdata);
		return;
	}

	QDateTime creation;
	QImage stateImage;

	unsigned width, height;
	thread->core->desiredVideoDimensions(thread->core, &width, &height);
	mStateExtdataItem item;
	if (mStateExtdataGet(&extdata, EXTDATA_SCREENSHOT, &item) && item.size >= width * height * 4) {
		stateImage = QImage((uchar*) item.data, width, height, QImage::Format_ARGB32).rgbSwapped();
	}

	if (mStateExtdataGet(&extdata, EXTDATA_META_TIME, &item) && item.size == sizeof(uint64_t)) {
		uint64_t creationUsec;
		LOAD_64LE(creationUsec, 0, item.data);
		creation = QDateTime::fromMSecsSinceEpoch(creationUsec / 1000LL);
	}

	if (!stateImage.isNull()) {
		QPixmap statePixmap;
		statePixmap.convertFromImage(stateImage);
		m_slots[slot - 1]->setIcon(statePixmap);
	}
	if (creation.toMSecsSinceEpoch()) {
		m_slots[slot - 1]->setText(creation.toString(Qt::DefaultLocaleShortDate));
	} else if (stateImage.isNull()) {
		m_slots[slot - 1]->setText(tr("Slot %1").arg(slot));
	} else {
		m_slots[slot - 1]->setText(QString());
	}
	vf->close(vf);
	mappedMemoryFree(state, thread->core->stateSize(thread->core));
}

void LoadSaveState::triggerState(int slot) {
	if (m_mode == LoadSave::SAVE) {
		m_controller->saveState(slot);
	} else {
		m_controller->loadState(slot);
	}
	close();
}

void LoadSaveState::closeEvent(QCloseEvent* event) {
	emit closed();
	QWidget::closeEvent(event);
}

void LoadSaveState::showEvent(QShowEvent* event) {
	m_slots[m_currentFocus]->setFocus();
	QWidget::showEvent(event);
}

void LoadSaveState::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	QRect full(QPoint(), size());
	painter.fillRect(full, QColor(0, 0, 0, 128));
}
