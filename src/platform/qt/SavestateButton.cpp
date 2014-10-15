#include "SavestateButton.h"

#include <QApplication>
#include <QPainter>

using namespace QGBA;

SavestateButton::SavestateButton(QWidget* parent)
	: QAbstractButton(parent)
{
	// Nothing to do
}

void SavestateButton::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	QRect frame(0, 0, width(), height());
	QRect full(1, 1, width() - 2, height() - 2);
	QPalette palette = QApplication::palette(this);
	painter.setPen(Qt::black);
	QLinearGradient grad(0, 0, 0, 1);
	grad.setCoordinateMode(QGradient::ObjectBoundingMode);
	grad.setColorAt(0, palette.color(QPalette::Shadow));
	grad.setColorAt(1, palette.color(QPalette::Dark));
	painter.setBrush(grad);
	painter.drawRect(frame);
	painter.setPen(Qt::NoPen);
	painter.drawPixmap(full, icon().pixmap(full.size()));
	if (hasFocus()) {
		QColor highlight = palette.color(QPalette::Highlight);
		highlight.setAlpha(128);
		painter.fillRect(full, highlight);
	}
}
