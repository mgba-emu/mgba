#pragma once

#ifdef USE_LIBMOBILE

#include <QDialog>

#include <memory>

#include <mgba/core/interface.h>

#include "ui_MobileAdapterView.h"

namespace QGBA {

class CoreController;
class Window;

class MobileAdapterView : public QDialog {
Q_OBJECT

public:
	MobileAdapterView(std::shared_ptr<CoreController> controller, Window* window, QWidget* parent = nullptr);
	~MobileAdapterView();

public slots:
	void setType(int type);
	void setUnmetered(bool unmetered);
	void setDns1();
	void setDns2();
	void setPort(int port);
	void setRelay();
	void setToken();
	void copyToken(bool checked);

private slots:
	void getConfig();
	void advanceFrameCounter();

private:
	Ui::MobileAdapterView m_ui;

	std::shared_ptr<CoreController> m_controller;

	Window* m_window;
};

}

#endif /* defined(USE_LIBMOBILE) */
