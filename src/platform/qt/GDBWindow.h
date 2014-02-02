#ifndef QGBA_GDB_WINDOW
#define QGBA_GDB_WINDOW

#include <QWidget>

class QLineEdit;
class QPushButton;

namespace QGBA {

class GDBController;

class GDBWindow : public QWidget {
Q_OBJECT

public:
	GDBWindow(GDBController* controller, QWidget* parent = nullptr);

private slots:
	void portChanged(const QString&);
	void bindAddressChanged(const QString&);

	void started();
	void stopped();

private:
	GDBController* m_gdbController;

	QLineEdit* m_portEdit;
	QLineEdit* m_bindAddressEdit;
	QPushButton* m_startStopButton;
};

}

#endif
