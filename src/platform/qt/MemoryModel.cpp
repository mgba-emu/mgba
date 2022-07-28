/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MemoryModel.h"

#include "GBAApp.h"
#include "CoreController.h"
#include "LogController.h"
#include "VFileDevice.h"
#include "utils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QPainter>
#include <QScrollBar>
#include <QSlider>
#include <QWheelEvent>

#include <mgba/core/core.h>
#include <mgba-util/vfs.h>

using namespace QGBA;

MemoryModel::MemoryModel(QWidget* parent)
	: QAbstractScrollArea(parent)
{
	m_font = GBAApp::app()->monospaceFont();
#ifdef Q_OS_MAC
	m_font.setPointSize(12);
#else
	m_font.setPointSize(10);
#endif
	QFontMetrics metrics(m_font);
	m_cellHeight = metrics.height();
	m_letterWidth = metrics.averageCharWidth();

	setFocusPolicy(Qt::StrongFocus);
	setContextMenuPolicy(Qt::ActionsContextMenu);

	QAction* copy = new QAction(tr("Copy selection"), this);
	copy->setShortcut(QKeySequence::Copy);
	connect(copy, &QAction::triggered, this, &MemoryModel::copy);
	addAction(copy);

	QAction* save = new QAction(tr("Save selection"), this);
	save->setShortcut(QKeySequence::Save);
	connect(save, &QAction::triggered, this, &MemoryModel::save);
	addAction(save);

	QAction* paste = new QAction(tr("Paste"), this);
	paste->setShortcut(QKeySequence::Paste);
	connect(paste, &QAction::triggered, this, &MemoryModel::paste);
	addAction(paste);

	QAction* load = new QAction(tr("Load"), this);
	load->setShortcut(QKeySequence::Open);
	connect(load, &QAction::triggered, this, &MemoryModel::load);
	addAction(load);

	static QString arg("%0");
	for (int i = 0; i < 256; ++i) {
		QStaticText str(arg.arg(i, 2, 16, QChar('0')).toUpper());
		str.prepare(QTransform(), m_font);
		m_staticNumbers.append(str);
	}

	for (int i = 0; i < 256; ++i) {
		QChar c(i);
		if (!c.isPrint()) {
			c = '.';
		}
		QStaticText str = QStaticText(QString(c));
		str.prepare(QTransform(), m_font);
		m_staticLatin1.append(str);
	}

	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_margins = QMargins(3, m_cellHeight + 1, 3, 0);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
	m_margins += QMargins(metrics.horizontalAdvance("0FFFFFF0 "), 0, metrics.horizontalAdvance(" AAAAAAAAAAAAAAAA"), 0);
#else
	m_margins += QMargins(metrics.width("0FFFFFF0 "), 0, metrics.width(" AAAAAAAAAAAAAAAA"), 0);
#endif
	m_cellSize = QSizeF((viewport()->size().width() - (m_margins.left() + m_margins.right())) / 16.0, m_cellHeight);

	connect(verticalScrollBar(), &QSlider::sliderMoved, [this](int position) {
		m_top = position;
		update();
	});

	connect(verticalScrollBar(), &QSlider::actionTriggered, [this](int action) {
		if (action == QSlider::SliderSingleStepAdd) {
			++m_top;
		} else if (action == QSlider::SliderSingleStepSub) {
			--m_top;
		} else if (action == QSlider::SliderPageStepAdd) {
			m_top += (viewport()->size().height() - m_cellHeight) / m_cellHeight;
		} else if (action == QSlider::SliderPageStepSub) {
			m_top -= (viewport()->size().height() - m_cellHeight) / m_cellHeight;
		} else {
			return;
		}
		boundsCheck();
		verticalScrollBar()->setValue(m_top);
		update();
	});

	setRegion(0, 0x10000000, tr("All"));
}

void MemoryModel::setController(std::shared_ptr<CoreController> controller) {
	m_core = controller->thread()->core;
}

void MemoryModel::setRegion(uint32_t base, uint32_t size, const QString& name, int segment) {
	m_top = 0;
	m_base = base;
	m_size = size;
	m_regionName = QStaticText(name);
	m_regionName.prepare(QTransform(), m_font);
	m_currentBank = segment;
	verticalScrollBar()->setRange(0, (size >> 4) + 1 - viewport()->size().height() / m_cellHeight);
	verticalScrollBar()->setValue(0);
	viewport()->update();
}

void MemoryModel::setSegment(int segment) {
	m_currentBank = segment;
	viewport()->update();
}

void MemoryModel::setAlignment(int width) {
	if (width != 1 && width != 2 && width != 4) {
		return;
	}
	m_align = width;
	m_buffer = 0;
	m_bufferedNybbles = 0;
	viewport()->update();
}

void MemoryModel::loadTBLFromPath(const QString& path) {
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		return;
	}
	m_codec = std::unique_ptr<TextCodec, TextCodecFree>(new TextCodec);
	TextCodecLoadTBL(m_codec.get(), vf, true);
	vf->close(vf);
}

void MemoryModel::loadTBL() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Load TBL"));
	if (filename.isNull()) {
		return;
	}
	loadTBLFromPath(filename);
}

void MemoryModel::jumpToAddress(const QString& hex) {
	bool ok = false;
	uint32_t i = hex.toInt(&ok, 16);
	if (ok) {
		jumpToAddress(i);
	}
}

void MemoryModel::jumpToAddress(uint32_t address) {
	m_top = (address - m_base) / 16;
	boundsCheck();
	verticalScrollBar()->setValue(m_top);
	m_buffer = 0;
	m_bufferedNybbles = 0;
}

void MemoryModel::copy() {
	QClipboard* clipboard = QApplication::clipboard();
	if (!clipboard) {
		return;
	}
	QByteArray bytestring(serialize());
	QString string;
	string.reserve(bytestring.size() * 2);
	static QString arg("%0");
	static QChar c0('0');
	for (uchar c : bytestring) {
		string.append(arg.arg(c, 2, 16, c0).toUpper());
	}
	clipboard->setText(string);
}

void MemoryModel::paste() {
	QClipboard* clipboard = QApplication::clipboard();
	if (!clipboard) {
		return;
	}
	QString string = clipboard->text();
	if (string.isEmpty()) {
		return;
	}
	QByteArray bytestring(QByteArray::fromHex(string.toLocal8Bit()));
	deserialize(bytestring);
	viewport()->update();
}

void MemoryModel::save() {
	QString filename = GBAApp::app()->getSaveFileName(this, tr("Save selected memory"));
	if (filename.isNull()) {
		return;
	}
	QFile outfile(filename);
	if (!outfile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		LOG(QT, WARN) << tr("Failed to open output file: %1").arg(filename);
		return;
	}
	QByteArray out(serialize());
	outfile.write(out);
}

void MemoryModel::load() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Load memory"));
	if (filename.isNull()) {
		return;
	}
	QFile infile(filename);
	if (!infile.open(QIODevice::ReadOnly)) {
		LOG(QT, WARN) << tr("Failed to open input file: %1").arg(filename);
		return;
	}
	QByteArray bytestring(infile.readAll());
	deserialize(bytestring);
	viewport()->update();
}

QByteArray MemoryModel::serialize() {
	QByteArray bytes;
	bytes.reserve(m_selection.second - m_selection.first);
	switch (m_align) {
	case 1:
		for (uint32_t i = m_selection.first; i < m_selection.second; i += m_align) {
			char datum = m_core->rawRead8(m_core, i, m_currentBank);
			bytes.append(datum);
		}
		break;
	case 2:
		for (uint32_t i = m_selection.first; i < m_selection.second; i += m_align) {
			quint16 datum = m_core->rawRead16(m_core, i, m_currentBank);
			char leDatum[2];
			STORE_16BE(datum, 0, (uint16_t*) leDatum);
			bytes.append(leDatum, 2);
		}
		break;
	case 4:
		for (uint32_t i = m_selection.first; i < m_selection.second; i += m_align) {
			quint32 datum = m_core->rawRead32(m_core, i, m_currentBank);
			char leDatum[4];
			STORE_32BE(datum, 0, (uint32_t*) leDatum);
			bytes.append(leDatum, 4);
		}
		break;
	}
	return bytes;
}

void MemoryModel::deserialize(const QByteArray& bytes) {
	uint32_t addr = m_selection.first;
	switch (m_align) {
	case 1:
		for (int i = 0; i < bytes.size(); i += m_align, addr += m_align) {
			uint8_t datum = bytes[i];
			m_core->rawWrite8(m_core, addr, m_currentBank, datum);
		}
		break;
	case 2:
		for (int i = 0; i < bytes.size(); i += m_align, addr += m_align) {
			char leDatum[2]{ bytes[i], bytes[i + 1] };
			uint16_t datum;
			LOAD_16BE(datum, 0, leDatum);
			m_core->rawWrite16(m_core, addr, m_currentBank, datum);
		}
		break;
	case 4:
		for (int i = 0; i < bytes.size(); i += m_align, addr += m_align) {
			char leDatum[4]{ bytes[i], bytes[i + 1], bytes[i + 2], bytes[i + 3] };
			uint32_t datum;
			LOAD_32BE(datum, 0, leDatum);
			m_core->rawWrite32(m_core, addr, m_currentBank, datum);
		}
		break;
	}
}

QString MemoryModel::decodeText(const QByteArray& bytes) {
	QString text;
	if (m_codec) {
		QByteArray array;
		TextCodecIterator iter;
		TextCodecStartDecode(m_codec.get(), &iter);
		uint8_t lineBuffer[128];
		for (quint8 byte : bytes) {
			ssize_t size = TextCodecAdvance(&iter, byte, lineBuffer, sizeof(lineBuffer));
			if (size > (ssize_t) sizeof(lineBuffer)) {
				size = sizeof(lineBuffer);
			}
			for (ssize_t i = 0; i < size; ++i) {
				array.append(lineBuffer[i]);
			}
		}
		ssize_t size = TextCodecFinish(&iter, lineBuffer, sizeof(lineBuffer));
		if (size > (ssize_t) sizeof(lineBuffer)) {
			size = sizeof(lineBuffer);
		}
		for (ssize_t i = 0; i < size; ++i) {
			array.append(lineBuffer[i]);
		}
		text = QString::fromUtf8(array);
	} else {
		for (uint8_t c : bytes) {
			text.append((uchar) c);
		}
	}
	return text;
}

void MemoryModel::resizeEvent(QResizeEvent*) {
	m_cellSize = QSizeF((viewport()->size().width() - (m_margins.left() + m_margins.right())) / 16.0, m_cellHeight);
	verticalScrollBar()->setRange(0, (m_size >> 4) + 1 - viewport()->size().height() / m_cellHeight);
	boundsCheck();
}

void MemoryModel::paintEvent(QPaintEvent*) {
	QPainter painter(viewport());
	QPalette palette;
	painter.setFont(m_font);
	painter.setPen(palette.color(QPalette::WindowText));
	static QChar c0('0');
	static QString arg("%0");
	static QString arg2("%0:%1");
	painter.drawStaticText(QPointF((m_margins.left() - m_regionName.size().width() - 1) / 2.0, 0), m_regionName);
	painter.drawText(
	    QRect(QPoint(viewport()->size().width() - m_margins.right(), 0), QSize(m_margins.right(), m_margins.top())),
	    Qt::AlignHCenter, m_codec ? tr("TBL") : tr("ISO-8859-1"));
	for (int x = 0; x < 16; ++x) {
		painter.drawText(QRectF(QPointF(m_cellSize.width() * x + m_margins.left(), 0), m_cellSize), Qt::AlignHCenter,
		                 QString::number(x, 16).toUpper());
	}
	int height = (viewport()->size().height() - m_cellHeight) / m_cellHeight;
	for (int y = 0; y < height; ++y) {
		int yp = m_cellHeight * y + m_margins.top();
		if ((y + m_top) * 16U >= m_size) {
			break;
		}
		QString data;
		if (m_currentBank >= 0) {
			data = arg2.arg(m_currentBank, 2, 16, c0).arg((y + m_top) * 16 + m_base, 4, 16, c0).toUpper();
		} else {
			data = arg.arg((y + m_top) * 16 + m_base, 8, 16, c0).toUpper();
		}
		painter.drawText(QRectF(QPointF(0, yp), QSizeF(m_margins.left(), m_cellHeight)), Qt::AlignHCenter, data);
		switch (m_align) {
		case 2:
			for (int x = 0; x < 16; x += 2) {
				uint32_t address = (y + m_top) * 16 + x + m_base;
				if (address >= m_base + m_size) {
					break;
				}
				if (isInSelection(address)) {
					painter.fillRect(QRectF(QPointF(m_cellSize.width() * x + m_margins.left(), yp),
					                        QSizeF(m_cellSize.width() * 2, m_cellSize.height())),
					                 palette.highlight());
					painter.setPen(palette.color(QPalette::HighlightedText));
					if (isEditing(address)) {
						drawEditingText(
						    painter,
						    QPointF(m_cellSize.width() * (x + 1.0) - 2 * m_letterWidth + m_margins.left(), yp));
						continue;
					}
				} else {
					painter.setPen(palette.color(QPalette::WindowText));
				}
				uint16_t b = m_core->rawRead16(m_core, address, m_currentBank);
				painter.drawStaticText(
				    QPointF(m_cellSize.width() * (x + 1.0) - 2 * m_letterWidth + m_margins.left(), yp),
				    m_staticNumbers[(b >> 8) & 0xFF]);
				painter.drawStaticText(QPointF(m_cellSize.width() * (x + 1.0) + m_margins.left(), yp),
				                       m_staticNumbers[b & 0xFF]);
			}
			break;
		case 4:
			for (int x = 0; x < 16; x += 4) {
				uint32_t address = (y + m_top) * 16 + x + m_base;
				if (address >= m_base + m_size) {
					break;
				}
				if (isInSelection(address)) {
					painter.fillRect(QRectF(QPointF(m_cellSize.width() * x + m_margins.left(), yp),
					                        QSizeF(m_cellSize.width() * 4, m_cellSize.height())),
					                 palette.highlight());
					painter.setPen(palette.color(QPalette::HighlightedText));
					if (isEditing(address)) {
						drawEditingText(
						    painter,
						    QPointF(m_cellSize.width() * (x + 2.0) - 4 * m_letterWidth + m_margins.left(), yp));
						continue;
					}
				} else {
					painter.setPen(palette.color(QPalette::WindowText));
				}
				uint32_t b = m_core->rawRead32(m_core, address, m_currentBank);
				painter.drawStaticText(
				    QPointF(m_cellSize.width() * (x + 2.0) - 4 * m_letterWidth + m_margins.left(), yp),
				    m_staticNumbers[(b >> 24) & 0xFF]);
				painter.drawStaticText(
				    QPointF(m_cellSize.width() * (x + 2.0) - 2 * m_letterWidth + m_margins.left(), yp),
				    m_staticNumbers[(b >> 16) & 0xFF]);
				painter.drawStaticText(QPointF(m_cellSize.width() * (x + 2.0) + m_margins.left(), yp),
				                       m_staticNumbers[(b >> 8) & 0xFF]);
				painter.drawStaticText(
				    QPointF(m_cellSize.width() * (x + 2.0) + 2 * m_letterWidth + m_margins.left(), yp),
				    m_staticNumbers[b & 0xFF]);
			}
			break;
		case 1:
		default:
			for (int x = 0; x < 16; ++x) {
				uint32_t address = (y + m_top) * 16 + x + m_base;
				if (address >= m_base + m_size) {
					break;
				}
				if (isInSelection(address)) {
					painter.fillRect(QRectF(QPointF(m_cellSize.width() * x + m_margins.left(), yp), m_cellSize),
					                 palette.highlight());
					painter.setPen(palette.color(QPalette::HighlightedText));
					if (isEditing(address)) {
						drawEditingText(painter,
						                QPointF(m_cellSize.width() * (x + 0.5) - m_letterWidth + m_margins.left(), yp));
						continue;
					}
				} else {
					painter.setPen(palette.color(QPalette::WindowText));
				}
				uint8_t b = m_core->rawRead8(m_core, address, m_currentBank);
				painter.drawStaticText(QPointF(m_cellSize.width() * (x + 0.5) - m_letterWidth + m_margins.left(), yp),
				                       m_staticNumbers[b]);
			}
			break;
		}
		painter.setPen(palette.color(QPalette::WindowText));
		for (int x = 0; x < 16; x += m_align) {
			QByteArray array;
			uint32_t b;
			switch (m_align) {
			case 1:
				b = m_core->rawRead8(m_core, (y + m_top) * 16 + x + m_base, m_currentBank);
				array.append((char) b);
				break;
			case 2:
				b = m_core->rawRead16(m_core, (y + m_top) * 16 + x + m_base, m_currentBank);
				array.append((char) b);
				array.append((char) (b >> 8));
				break;
			case 4:
				b = m_core->rawRead32(m_core, (y + m_top) * 16 + x + m_base, m_currentBank);
				array.append((char) b);
				array.append((char) (b >> 8));
				array.append((char) (b >> 16));
				array.append((char) (b >> 24));
				break;
			}
			QString unfilteredText = decodeText(array);
			QString text;
			if (unfilteredText.isEmpty()) {
				text.fill('.', m_align);
			} else {
				for (QChar c : unfilteredText) {
					if (!c.isPrint()) {
						text.append(QChar('.'));
					} else {
						text.append(c);
					}
				}
			}
			for (int i = 0; i < text.size() && i < m_align; ++i) {
				const QChar c = text.at(i);
				const QPointF location(viewport()->size().width() - (16 - x - i) * m_margins.right() / 17.0 - m_letterWidth * 0.5, yp);
				if (c < 256) {
					painter.drawStaticText(location, m_staticLatin1[c.cell()]);
				} else {
					painter.drawText(location, c);
				}
			}
		}
	}
	painter.drawLine(m_margins.left(), 0, m_margins.left(), viewport()->size().height());
	painter.drawLine(viewport()->size().width() - m_margins.right(), 0, viewport()->size().width() - m_margins.right(),
	                 viewport()->size().height());
	painter.drawLine(0, m_margins.top(), viewport()->size().width(), m_margins.top());
}

void MemoryModel::wheelEvent(QWheelEvent* event) {
	m_top -= event->angleDelta().y() / 8;
	boundsCheck();
	event->accept();
	verticalScrollBar()->setValue(m_top);
	update();
}

void MemoryModel::mousePressEvent(QMouseEvent* event) {
	if (event->x() < m_margins.left() || event->y() < m_margins.top() ||
	    event->x() > size().width() - m_margins.right()) {
		m_selection = qMakePair(0, 0);
		return;
	}

	QPoint position(event->pos() - QPoint(m_margins.left(), m_margins.top()));
	uint32_t address = int(position.x() / m_cellSize.width()) +
	                   (int(position.y() / m_cellSize.height()) + m_top) * 16 + m_base;
	if (event->button() == Qt::RightButton && isInSelection(address)) {
		return;
	}
	if (event->modifiers() & Qt::ShiftModifier) {
		if ((address & ~(m_align - 1)) < m_selectionAnchor) {
			m_selection = qMakePair(address & ~(m_align - 1), m_selectionAnchor + m_align);
		} else {
			m_selection = qMakePair(m_selectionAnchor, (address & ~(m_align - 1)) + m_align);
		}
	} else {
		m_selectionAnchor = address & ~(m_align - 1);
		m_selection = qMakePair(m_selectionAnchor, m_selectionAnchor + m_align);
	}
	m_buffer = 0;
	m_bufferedNybbles = 0;
	emit selectionChanged(m_selection.first, m_selection.second);
	viewport()->update();
}

void MemoryModel::mouseMoveEvent(QMouseEvent* event) {
	if (event->x() < m_margins.left() || event->y() < m_margins.top() ||
	    event->x() > size().width() - m_margins.right()) {
		return;
	}

	QPoint position(event->pos() - QPoint(m_margins.left(), m_margins.top()));
	uint32_t address = int(position.x() / m_cellSize.width()) +
	                   (int(position.y() / m_cellSize.height()) + m_top) * 16 + m_base;
	if ((address & ~(m_align - 1)) < m_selectionAnchor) {
		m_selection = qMakePair(address & ~(m_align - 1), m_selectionAnchor + m_align);
	} else {
		m_selection = qMakePair(m_selectionAnchor, (address & ~(m_align - 1)) + m_align);
	}
	m_buffer = 0;
	m_bufferedNybbles = 0;
	emit selectionChanged(m_selection.first, m_selection.second);
	viewport()->update();
}

void MemoryModel::keyPressEvent(QKeyEvent* event) {
	if (m_selection.first >= m_selection.second) {
		return;
	}
	int key = event->key();
	uint8_t nybble = 0;
	switch (key) {
	case Qt::Key_0:
	case Qt::Key_1:
	case Qt::Key_2:
	case Qt::Key_3:
	case Qt::Key_4:
	case Qt::Key_5:
	case Qt::Key_6:
	case Qt::Key_7:
	case Qt::Key_8:
	case Qt::Key_9:
		nybble = key - Qt::Key_0;
		break;
	case Qt::Key_A:
	case Qt::Key_B:
	case Qt::Key_C:
	case Qt::Key_D:
	case Qt::Key_E:
	case Qt::Key_F:
		nybble = key - Qt::Key_A + 10;
		break;
	case Qt::Key_Left:
		adjustCursor(-m_align, event->modifiers() & Qt::ShiftModifier);
		return;
	case Qt::Key_Right:
		adjustCursor(m_align, event->modifiers() & Qt::ShiftModifier);
		return;
	case Qt::Key_Up:
		adjustCursor(-16, event->modifiers() & Qt::ShiftModifier);
		return;
	case Qt::Key_Down:
		adjustCursor(16, event->modifiers() & Qt::ShiftModifier);
		return;
	case Qt::Key_PageUp:
		adjustCursor(-16 * ((viewport()->size().height() - m_cellHeight) / m_cellHeight), event->modifiers() & Qt::ShiftModifier);
		return;
	case Qt::Key_PageDown:
		adjustCursor(16 * ((viewport()->size().height() - m_cellHeight) / m_cellHeight), event->modifiers() & Qt::ShiftModifier);
		return;
	default:
		return;
	}
	m_buffer <<= 4;
	m_buffer |= nybble;
	++m_bufferedNybbles;
	if (m_bufferedNybbles == m_align * 2) {
		switch (m_align) {
		case 1:
			m_core->rawWrite8(m_core, m_selection.first, m_currentBank, m_buffer);
			break;
		case 2:
			m_core->rawWrite16(m_core, m_selection.first, m_currentBank, m_buffer);
			break;
		case 4:
			m_core->rawWrite32(m_core, m_selection.first, m_currentBank, m_buffer);
			break;
		}
		m_bufferedNybbles = 0;
		m_buffer = 0;
		m_selection.first += m_align;
		if (m_selection.second <= m_selection.first) {
			m_selection.second = m_selection.first + m_align;
		}
		emit selectionChanged(m_selection.first, m_selection.second);
	}
	viewport()->update();
}

void MemoryModel::boundsCheck() {
	m_top = clamp(m_top, 0, static_cast<int32_t>(m_size >> 4) + 1 - viewport()->size().height() / m_cellHeight);
}

bool MemoryModel::isInSelection(uint32_t address) {
	if (m_selection.first == m_selection.second) {
		return false;
	}
	if (m_selection.second <= (address | (m_align - 1))) {
		return false;
	}
	if (m_selection.first <= (address & ~(m_align - 1))) {
		return true;
	}
	return false;
}

bool MemoryModel::isEditing(uint32_t address) {
	return m_bufferedNybbles && m_selection.first == (address & ~(m_align - 1));
}

void MemoryModel::drawEditingText(QPainter& painter, const QPointF& origin) {
	QPointF o(origin);
	for (int nybbles = m_bufferedNybbles; nybbles > 0; nybbles -= 2) {
		if (nybbles > 1) {
			uint8_t b = m_buffer >> ((nybbles - 2) * 4);
			painter.drawStaticText(o, m_staticNumbers[b]);
		} else {
			int b = m_buffer & 0xF;
			if (b < 10) {
				painter.drawStaticText(o, m_staticLatin1[b + '0']);
			} else {
				painter.drawStaticText(o, m_staticLatin1[b - 10 + 'A']);
			}
		}
		o += QPointF(m_letterWidth * 2, 0);
	}
}

void MemoryModel::adjustCursor(int adjust, bool shift) {
	if (m_selection.first >= m_selection.second) {
		return;
	}
	int cursorPosition = m_top;
	if (shift) {
		uint32_t absolute = adjust;
		if (m_selectionAnchor == m_selection.first) {
			if (adjust < 0 && m_base - adjust > m_selection.second) {
				absolute = m_base - m_selection.second + m_align;
			} else if (adjust > 0 && m_selection.second + adjust >= m_base + m_size) {
				absolute = m_base + m_size - m_selection.second;
			}
			absolute += m_selection.second;
			if (absolute <= m_selection.first) {
				m_selection.second = m_selection.first + m_align;
				m_selection.first = absolute - m_align;
				cursorPosition = m_selection.first;
			} else {
				m_selection.second = absolute;
				cursorPosition = m_selection.second - m_align;
			}
		} else {
			if (adjust < 0 && m_base - adjust > m_selection.first) {
				absolute = m_base - m_selection.first;
			} else if (adjust > 0 && m_selection.first + adjust >= m_base + m_size) {
				absolute = m_base + m_size - m_selection.first - m_align;
			}
			absolute += m_selection.first;
			if (absolute >= m_selection.second) {
				m_selection.first = m_selection.second - m_align;
				m_selection.second = absolute + m_align;
				cursorPosition = absolute;
			} else {
				m_selection.first = absolute;
				cursorPosition = m_selection.first;
			}
		}
		cursorPosition = (cursorPosition - m_base) / 16;
	} else {
		if (m_selectionAnchor == m_selection.first) {
			m_selectionAnchor = m_selection.second - m_align;
		} else {
			m_selectionAnchor = m_selection.first;
		}
		if (adjust < 0 && m_base - adjust > m_selectionAnchor) {
			m_selectionAnchor = m_base;
		} else if (adjust > 0 && m_selectionAnchor + adjust >= m_base + m_size) {
			m_selectionAnchor = m_base + m_size - m_align;
		} else {
			m_selectionAnchor += adjust;
		}
		m_selection.first = m_selectionAnchor;
		m_selection.second = m_selection.first + m_align;
		cursorPosition = (m_selectionAnchor - m_base) / 16;
	}
	if (cursorPosition < m_top) {
		m_top = cursorPosition;
	} else if (cursorPosition >= m_top + viewport()->size().height() / m_cellHeight - 1) {
		m_top = cursorPosition - viewport()->size().height() / m_cellHeight + 2;
	}
	emit selectionChanged(m_selection.first, m_selection.second);
	viewport()->update();
}

void MemoryModel::TextCodecFree::operator()(TextCodec* codec) {
	TextCodecDeinit(codec);
	delete(codec);
}
