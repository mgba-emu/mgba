/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "FrameView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPalette>

#include <array>

#include "CoreController.h"

#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/video.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/memory.h>
#endif

using namespace QGBA;

FrameView::FrameView(std::shared_ptr<CoreController> controller, QWidget* parent)
	: AssetView(controller, parent)
{
	m_ui.setupUi(this);

	m_glowTimer.setInterval(33);
	connect(&m_glowTimer, &QTimer::timeout, this, [this]() {
		++m_glowFrame;
		invalidateQueue();
	});
	m_glowTimer.start();

	m_ui.compositedView->installEventFilter(this);

	connect(m_ui.queue, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
		Layer& layer = m_queue[item->data(Qt::UserRole).toInt()];
		layer.enabled = item->checkState() == Qt::Checked;
		if (layer.enabled) {
			m_disabled.remove(layer.id);
		} else {
			m_disabled.insert(layer.id);			
		}
		invalidateQueue();
	});
	connect(m_ui.queue, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
		if (item) {
			m_active = m_queue[item->data(Qt::UserRole).toInt()].id;
		} else {
			m_active = {};
		}
		invalidateQueue();
	});
}

void FrameView::selectLayer(const QPointF& coord) {
	for (const Layer& layer : m_queue) {
		QPointF location = layer.location;
		QSizeF layerDims(layer.image.width(), layer.image.height());
		QRegion region;
		if (layer.repeats) {
			if (location.x() + layerDims.width() < 0) {
				location.setX(std::fmod(location.x(), layerDims.width()));
			}
			if (location.y() + layerDims.height() < 0) {
				location.setY(std::fmod(location.y(), layerDims.height()));
			}

			region += layer.mask.translated(location.x(), location.y());
			region += layer.mask.translated(location.x() + layerDims.width(), location.y());
			region += layer.mask.translated(location.x(), location.y() + layerDims.height());
			region += layer.mask.translated(location.x() + layerDims.width(), location.y() + layerDims.height());
		} else {
			region = layer.mask.translated(location.x(), location.y());
		}

		if (region.contains(QPoint(coord.x(), coord.y()))) {
			m_active = layer.id;
			m_glowFrame = 0;
			break;
		}
	}
}

void FrameView::updateTilesGBA(bool force) {
	if (m_ui.freeze->checkState() == Qt::Checked) {
		return;
	}
	m_queue.clear();
	{
		CoreController::Interrupter interrupter(m_controller);
		updateRendered();

		uint16_t* io = static_cast<GBA*>(m_controller->thread()->core->board)->memory.io;
		int mode = GBARegisterDISPCNTGetMode(io[REG_DISPCNT >> 1]);

		std::array<bool, 4> enabled{
			GBARegisterDISPCNTIsBg0Enable(io[REG_DISPCNT >> 1]),
			GBARegisterDISPCNTIsBg1Enable(io[REG_DISPCNT >> 1]),
			GBARegisterDISPCNTIsBg2Enable(io[REG_DISPCNT >> 1]),
			GBARegisterDISPCNTIsBg3Enable(io[REG_DISPCNT >> 1]),
		};

		for (int priority = 0; priority < 4; ++priority) {
			for (int sprite = 0; sprite < 128; ++sprite) {
				ObjInfo info;
				lookupObj(sprite, &info);

				if (!info.enabled || info.priority != priority) {
					continue;
				}

				QPointF offset(info.x, info.y);
				QImage obj(compositeObj(info));
				if (info.hflip || info.vflip) {
					obj = obj.mirrored(info.hflip, info.vflip);
				}
				m_queue.append({
					{ LayerId::SPRITE, sprite },
					!m_disabled.contains({ LayerId::SPRITE, sprite}),
					QPixmap::fromImage(obj),
					{}, offset, false
				});
				if (m_queue.back().image.hasAlpha()) {
					m_queue.back().mask = QRegion(m_queue.back().image.mask());
				} else {
					m_queue.back().mask = QRegion(0, 0, m_queue.back().image.width(), m_queue.back().image.height());
				}
			}

			for (int bg = 0; bg < 4; ++bg) {
				if (!enabled[bg]) {
					continue;
				}
				if (GBARegisterBGCNTGetPriority(io[(REG_BG0CNT >> 1) + bg]) != priority) {
					continue;
				}

				QPointF offset;
				if (mode == 0) {
					offset.setX(-(io[(REG_BG0HOFS >> 1) + (bg << 1)] & 0x1FF));
					offset.setY(-(io[(REG_BG0VOFS >> 1) + (bg << 1)] & 0x1FF));
				};
				m_queue.append({
					{ LayerId::BACKGROUND, bg },
					!m_disabled.contains({ LayerId::BACKGROUND, bg}),
					QPixmap::fromImage(compositeMap(bg, m_mapStatus[bg])),
					{}, offset, true
				});
				if (m_queue.back().image.hasAlpha()) {
					m_queue.back().mask = QRegion(m_queue.back().image.mask());
				} else {
					m_queue.back().mask = QRegion(0, 0, m_queue.back().image.width(), m_queue.back().image.height());
				}
			}
		}
	}
	invalidateQueue(QSize(GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS));
}

void FrameView::updateTilesGB(bool force) {
	if (m_ui.freeze->checkState() == Qt::Checked) {
		return;
	}
	m_queue.clear();
	{
		CoreController::Interrupter interrupter(m_controller);
		updateRendered();
	}
	invalidateQueue(m_controller->screenDimensions());
}

void FrameView::invalidateQueue(const QSize& dims) {
	QSize realDims = dims;
	if (!dims.isValid()) {
		realDims = m_composited.size() / m_ui.magnification->value();
	}
	bool blockSignals = m_ui.queue->blockSignals(true);
	QPixmap composited(realDims);

	QPainter painter(&composited);
	QPalette palette;
	QColor activeColor = palette.color(QPalette::HighlightedText);
	activeColor.setAlpha(sin(m_glowFrame * M_PI / 60) * 16 + 96);

	QRectF rect(0, 0, realDims.width(), realDims.height());
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	painter.fillRect(rect, QColor(0, 0, 0, 0));

	painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
	for (int i = 0; i < m_queue.count(); ++i) {
		const Layer& layer = m_queue[i];
		QListWidgetItem* item;
		if (i >= m_ui.queue->count()) {
			item = new QListWidgetItem;
			m_ui.queue->addItem(item);
		} else {
			item = m_ui.queue->item(i);
		}
		item->setText(layer.id.readable());
		item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		item->setCheckState(layer.enabled ? Qt::Checked : Qt::Unchecked);
		item->setData(Qt::UserRole, i);
		item->setSelected(layer.id == m_active);

		if (!layer.enabled) {
			continue;
		}

		QPointF location = layer.location;
		QSizeF layerDims(layer.image.width(), layer.image.height());
		QRegion region;
		if (layer.repeats) {
			if (location.x() + layerDims.width() < 0) {
				location.setX(std::fmod(location.x(), layerDims.width()));
			}
			if (location.y() + layerDims.height() < 0) {
				location.setY(std::fmod(location.y(), layerDims.height()));
			}

			if (layer.id == m_active) {
				region = layer.mask.translated(location.x(), location.y());
				region += layer.mask.translated(location.x() + layerDims.width(), location.y());
				region += layer.mask.translated(location.x(), location.y() + layerDims.height());
				region += layer.mask.translated(location.x() + layerDims.width(), location.y() + layerDims.height());
			}
		} else {
			QRectF layerRect(location, layerDims);
			if (!rect.intersects(layerRect)) {
				continue;
			}
			if (layer.id == m_active) {
				region = layer.mask.translated(location.x(), location.y());
			}
		}

		if (layer.id == m_active) {
			painter.setClipping(true);
			painter.setClipRegion(region);
			painter.fillRect(rect, activeColor);
			painter.setClipping(false);
		}

		if (layer.repeats) {
			painter.drawPixmap(location, layer.image);
			painter.drawPixmap(location + QPointF(layerDims.width(), 0), layer.image);
			painter.drawPixmap(location + QPointF(0, layerDims.height()), layer.image);
			painter.drawPixmap(location + QPointF(layerDims.width(), layerDims.height()), layer.image);
		} else {
			painter.drawPixmap(location, layer.image);
		}
	}
	painter.end();

	while (m_ui.queue->count() > m_queue.count()) {
		delete m_ui.queue->takeItem(m_queue.count());
	}
	m_ui.queue->blockSignals(blockSignals);

	m_composited = composited.scaled(realDims * m_ui.magnification->value());
	m_ui.compositedView->setPixmap(m_composited);
}

void FrameView::updateRendered() {
	if (m_ui.freeze->checkState() == Qt::Checked) {
		return;
	}
	m_rendered.convertFromImage(m_controller->getPixels());
	m_rendered = m_rendered.scaledToHeight(m_rendered.height() * m_ui.magnification->value());
	m_ui.renderedView->setPixmap(m_rendered);
}

bool FrameView::eventFilter(QObject* obj, QEvent* event) {
	if (event->type() != QEvent::MouseButtonPress) {
		return false;
	}
	QPointF pos = static_cast<QMouseEvent*>(event)->localPos();
	pos /= m_ui.magnification->value();
	selectLayer(pos);
	return true;
}

QString FrameView::LayerId::readable() const {
	QString typeStr;
	switch (type) {
	case NONE:
		return tr("None");
	case BACKGROUND:
		typeStr = tr("Background");
		break;
	case WINDOW:
		typeStr = tr("Window");
		break;
	case SPRITE:
		typeStr = tr("Sprite");
		break;
	}
	if (index < 0) {
		return typeStr;
	}
	return tr("%1 %2").arg(typeStr).arg(index);
}