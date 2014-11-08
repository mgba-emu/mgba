#ifndef QGBA_GBA_KEY_EDITOR
#define QGBA_GBA_KEY_EDITOR

#include <QList>
#include <QPicture>
#include <QWidget>

class QPushButton;

namespace QGBA {

class InputController;
class KeyEditor;

class GBAKeyEditor : public QWidget {
Q_OBJECT

public:
	GBAKeyEditor(InputController* controller, int type, QWidget* parent = nullptr);

public slots:
	void setAll();

protected:
	virtual void resizeEvent(QResizeEvent*) override;
	virtual void paintEvent(QPaintEvent*) override;

private:
	static const qreal DPAD_CENTER_X;
	static const qreal DPAD_CENTER_Y;
	static const qreal DPAD_WIDTH;
	static const qreal DPAD_HEIGHT;

	void setNext();

	void setLocation(QWidget* widget, qreal x, qreal y);

	QPushButton* m_setAll;
	KeyEditor* m_keyDU;
	KeyEditor* m_keyDD;
	KeyEditor* m_keyDL;
	KeyEditor* m_keyDR;
	KeyEditor* m_keySelect;
	KeyEditor* m_keyStart;
	KeyEditor* m_keyA;
	KeyEditor* m_keyB;
	KeyEditor* m_keyL;
	KeyEditor* m_keyR;
	QList<KeyEditor*> m_keyOrder;
	QList<KeyEditor*>::iterator m_currentKey;

	InputController* m_controller;

	QPicture m_background;
};

}

#endif
