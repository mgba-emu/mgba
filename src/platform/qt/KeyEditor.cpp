#include "KeyEditor.h"

#include <QKeyEvent>

using namespace QGBA;

KeyEditor::KeyEditor(QWidget* parent)
	: QLineEdit(parent)
{
	setAlignment(Qt::AlignCenter);
}

void KeyEditor::setValue(int key) {
	setText(QKeySequence(key).toString(QKeySequence::NativeText));
	m_key = key;
	emit valueChanged(key);
}

QSize KeyEditor::sizeHint() const {
	QSize hint = QLineEdit::sizeHint();
	hint.setWidth(40);
	return hint;
}

void KeyEditor::keyPressEvent(QKeyEvent* event) {
	setValue(event->key());
	event->accept();
}
