#include "LoadSaveState.h"

#include "GameController.h"
#include "VFileDevice.h"

extern "C" {
#include "gba-serialize.h"
#include "gba-video.h"
}

using namespace QGBA;

LoadSaveState::LoadSaveState(GameController* controller, QWidget* parent)
	: QWidget(parent)
	, m_controller(controller)
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
		connect(m_slots[i], &QAbstractButton::clicked, this, [this, i]() { triggerState(i); });
	}
}

void LoadSaveState::setMode(LoadSave mode) {
	m_mode = mode;
	QString text = mode == LoadSave::LOAD ? tr("Load State") : tr("SaveState");
	setWindowTitle(text);
	m_ui.lsLabel->setText(text);
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
