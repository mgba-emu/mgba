#ifndef QGBA_LOAD_SAVE_STATE
#define QGBA_LOAD_SAVE_STATE

#include <QWidget>

#include "ui_LoadSaveState.h"

namespace QGBA {

class GameController;
class SavestateButton;

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

signals:
	void closed();

protected:
	virtual bool eventFilter(QObject*, QEvent*) override;
	virtual void closeEvent(QCloseEvent*) override;
	virtual void showEvent(QShowEvent*) override;

private:
	void loadState(int slot);
	void triggerState(int slot);

	Ui::LoadSaveState m_ui;
	GameController* m_controller;
	SavestateButton* m_slots[NUM_SLOTS];
	LoadSave m_mode;

	int m_currentFocus;
	QPixmap m_currentImage;
};

}

#endif
