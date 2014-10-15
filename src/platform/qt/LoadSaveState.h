#ifndef QGBA_LOAD_SAVE_STATE
#define QGBA_LOAD_SAVE_STATE

#include <QWidget>

#include "ui_LoadSaveState.h"

namespace QGBA {

class GameController;

enum class LoadSave {
	LOAD,
	SAVE
};

class LoadSaveState : public QWidget {
Q_OBJECT

public:

	const static int NUM_SLOTS = 9;

	LoadSaveState(GameController* controller, QWidget* parent = nullptr);

	void setMode(LoadSave mode);

private:
	void loadState(int slot);
	void triggerState(int slot);

	Ui::LoadSaveState m_ui;
	GameController* m_controller;
	QPushButton* m_slots[NUM_SLOTS];
	LoadSave m_mode;

	QPixmap m_currentImage;
};

}

#endif
