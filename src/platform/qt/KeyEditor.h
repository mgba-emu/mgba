#ifndef QGBA_KEY_EDITOR
#define QGBA_KEY_EDITOR

#include <QLineEdit>

namespace QGBA {

class KeyEditor : public QLineEdit {
Q_OBJECT

public:
	KeyEditor(QWidget* parent = nullptr);

	void setValue(int key);
	int value() const { return m_key; }

	void setNumeric(bool numeric) { m_numeric = numeric; }

	virtual QSize sizeHint() const override;

signals:
	void valueChanged(int key);

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;

private:
	int m_key;
	bool m_numeric;
};

}

#endif
