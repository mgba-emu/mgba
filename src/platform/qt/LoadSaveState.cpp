#include "LoadSaveState.h"

#include "GameController.h"
#include "VFileDevice.h"

#include <QKeyEvent>

extern "C" {
#include "gba-serialize.h"
#include "gba-video.h"
}

using namespace QGBA;

LoadSaveState::LoadSaveState(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
	, m_currentFocus(0)
{
	m_ui.setupUi(this);

	QImage currentImage(reinterpret_cast<const uchar*>(controller->drawContext()), VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, 1024, QImage::Format_RGB32);
	m_currentImage.convertFromImage(currentImage.rgbSwapped());

	m_slots[0] = m_ui.state1;
	m_slots[1] = m_ui.state2;
	m_slots[2] = m_ui.state3;
	m_slots[3] = m_ui.state4;
	m_slots[4] = m_ui.state5;
	m_slots[5] = m_ui.state6;
	m_slots[6] = m_ui.state7;
	m_slots[7] = m_ui.state8;
	m_slots[8] = m_ui.state9;

	int i;
	for (i = 0; i < NUM_SLOTS; ++i) {
		loadState(i);
		m_slots[i]->installEventFilter(this);
		connect(m_slots[i], &QAbstractButton::clicked, this, [this, i]() { triggerState(i); });
	}
}

void LoadSaveState::setMode(LoadSave mode) {
	m_mode = mode;
	QString text = mode == LoadSave::LOAD ? tr("Load State") : tr("SaveState");
	setWindowTitle(text);
	m_ui.lsLabel->setText(text);
}

bool LoadSaveState::eventFilter(QObject*, QEvent* event) {
	if (event->type() == QEvent::KeyPress) {
		int column = m_currentFocus % 3;
		int row = m_currentFocus - column;
		switch (static_cast<QKeyEvent*>(event)->key()) {
		case Qt::Key_Up:
			row += 6;
			break;
		case Qt::Key_Down:
			row += 3;
			break;
		case Qt::Key_Left:
			column += 2;
			break;
		case Qt::Key_Right:
			column += 1;
			break;
		default:
			return false;
		}
		column %= 3;
		row %= 9;
		m_currentFocus = column + row;
		m_slots[m_currentFocus]->setFocus();
		return true;
	}
	return false;
}

void LoadSaveState::loadState(int slot) {
	GBAThread* thread = m_controller->thread();
	VFile* vf = GBAGetState(thread->gba, thread->stateDir, slot, false);
	if (!vf) {
		return;
	}
	VFileDevice vdev(vf);
	QImage stateImage;
	stateImage.load(&vdev, "PNG");
	if (!stateImage.isNull()) {
		QPixmap statePixmap;
		statePixmap.convertFromImage(stateImage);
		m_slots[slot]->setIcon(statePixmap);
		m_slots[slot]->setText(QString());
	} else {
		m_slots[slot]->setText(tr("Slot %1").arg(slot + 1));
	}
	m_slots[slot]->setShortcut(QString::number(slot + 1));
}

void LoadSaveState::triggerState(int slot) {
	GBAThread* thread = m_controller->thread();
	GBAThreadInterrupt(thread);
	if (m_mode == LoadSave::SAVE) {
		GBASaveState(thread->gba, thread->stateDir, slot, true);
	} else {
		GBALoadState(thread->gba, thread->stateDir, slot);
	}
	GBAThreadContinue(thread);
	close();
}

void LoadSaveState::closeEvent(QCloseEvent* event) {
	emit closed();
	QWidget::closeEvent(event);
}
