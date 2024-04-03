/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DisplayQt.h"

#include "CoreController.h"
#include "utils.h"

#include <QPainter>

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba-util/math.h>

using namespace QGBA;

DisplayQt::DisplayQt(QWidget* parent)
	: Display(parent)
{
	m_backend.init = &DisplayQt::init;
	m_backend.deinit = &DisplayQt::deinit;
	m_backend.setLayerDimensions = &DisplayQt::setLayerDimensions;
	m_backend.layerDimensions = &DisplayQt::layerDimensions;
	m_backend.swap = &DisplayQt::swap;
	m_backend.clear = &DisplayQt::clear;
	m_backend.contextResized = &DisplayQt::contextResized;
	m_backend.setImageSize = &DisplayQt::setImageSize;
	m_backend.imageSize = &DisplayQt::imageSize;
	m_backend.setImage = &DisplayQt::setImage;
	m_backend.drawFrame = &DisplayQt::drawFrame;
	m_backend.filter = isFiltered();
	m_backend.lockAspectRatio = isAspectRatioLocked();
	m_backend.lockIntegerScaling = isIntegerScalingLocked();
	m_backend.interframeBlending = hasInterframeBlending();
	m_backend.user = this;
}

void DisplayQt::startDrawing(std::shared_ptr<CoreController> controller) {
	QSize size = controller->screenDimensions();
	m_width = size.width();
	m_height = size.height();
	m_oldBacking = QImage();
	m_isDrawing = true;
	m_context = std::move(controller);
	emit drawingStarted();
}

void DisplayQt::stopDrawing() {
	m_isDrawing = false;
	m_context.reset();
}

void DisplayQt::lockAspectRatio(bool lock) {
	Display::lockAspectRatio(lock);
	m_backend.lockAspectRatio = lock;
	update();
}

void DisplayQt::lockIntegerScaling(bool lock) {
	Display::lockIntegerScaling(lock);
	m_backend.lockIntegerScaling = lock;
	update();
}

void DisplayQt::interframeBlending(bool lock) {
	Display::interframeBlending(lock);
	m_backend.interframeBlending = lock;
	update();
}

void DisplayQt::filter(bool filter) {
	Display::filter(filter);
	m_backend.filter = filter;
	update();
}

void DisplayQt::framePosted() {
	update();
	const color_t* buffer = m_context->drawContext();
	if (const_cast<const QImage&>(m_layers[VIDEO_LAYER_IMAGE]).bits() == reinterpret_cast<const uchar*>(buffer)) {
		return;
	}
	m_oldBacking = m_layers[VIDEO_LAYER_IMAGE];
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	m_layers[VIDEO_LAYER_IMAGE] = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB16);
#else
	m_layers[VIDEO_LAYER_IMAGE] = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_RGB555);
#endif
#else
	m_layers[VIDEO_LAYER_IMAGE] = QImage(reinterpret_cast<const uchar*>(buffer), m_width, m_height, QImage::Format_ARGB32);
	m_layers[VIDEO_LAYER_IMAGE] = m_layers[VIDEO_LAYER_IMAGE].convertToFormat(QImage::Format_RGB32);
#endif
#ifndef COLOR_5_6_5
	m_layers[VIDEO_LAYER_IMAGE] = m_layers[VIDEO_LAYER_IMAGE].rgbSwapped();
#endif
	m_layerDims[VIDEO_LAYER_IMAGE].setWidth(m_width);
	m_layerDims[VIDEO_LAYER_IMAGE].setHeight(m_height);
	redoBounds();
}

void DisplayQt::resizeContext() {
	if (!m_context) {
		return;
	}
	QSize size = m_context->screenDimensions();
	if (m_width != size.width() || m_height != size.height()) {
		m_width = size.width();
		m_height = size.height();
		m_oldBacking = QImage();
		m_layers[VIDEO_LAYER_IMAGE] = QImage();
	}
}

void DisplayQt::setBackgroundImage(const QImage& image) {
	m_layers[VIDEO_LAYER_BACKGROUND] = image;
	redoBounds();
	update();
}

void DisplayQt::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	painter.fillRect(QRect(QPoint(), size()), Qt::black);
	if (isFiltered()) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform);
	}

	struct mRectangle frame;
	VideoBackendGetFrame(&m_backend, &frame);
	QPoint origin(-frame.x, -frame.y);
	QRect full(clampSize(contentSize(), size(), isAspectRatioLocked(), isIntegerScalingLocked()));
	painter.save();
	painter.translate(full.topLeft());
	painter.scale(full.width() / static_cast<qreal>(frame.width), full.height() / static_cast<qreal>(frame.height));

	if (!m_layers[VIDEO_LAYER_BACKGROUND].isNull()) {
		painter.drawImage(m_layerDims[VIDEO_LAYER_BACKGROUND].translated(origin), m_layers[VIDEO_LAYER_BACKGROUND]);
	}

	if (hasInterframeBlending()) {
		painter.drawImage(m_layerDims[VIDEO_LAYER_IMAGE].translated(origin), m_oldBacking, QRect(0, 0, m_width, m_height));
		painter.setOpacity(0.5);
	}
	painter.drawImage(m_layerDims[VIDEO_LAYER_IMAGE].translated(origin), m_layers[VIDEO_LAYER_IMAGE], QRect(0, 0, m_width, m_height));

	for (int i = VIDEO_LAYER_IMAGE + 1; i < VIDEO_LAYER_MAX; ++i) {
		if (m_layers[i].isNull()) {
			continue;
		}

		painter.drawImage(m_layerDims[i].translated(origin), m_layers[i]);
	}

	painter.restore();
	painter.setOpacity(1);
	if (isShowOSD() || isShowFrameCounter()) {
		messagePainter()->paint(&painter);
	}
}

void DisplayQt::redoBounds() {
	const static std::initializer_list<VideoLayer> centeredLayers{VIDEO_LAYER_BACKGROUND};
	mRectangle frame = {0};
	frame.width = m_width;
	frame.height = m_height;

	for (VideoLayer l : centeredLayers) {
		mRectangle dims{};
		dims.width = m_layers[l].width();
		dims.height = m_layers[l].height();
		mRectangleCenter(&frame, &dims);
		m_layerDims[l].setX(dims.x);
		m_layerDims[l].setY(dims.y);
		m_layerDims[l].setWidth(dims.width);
		m_layerDims[l].setHeight(dims.height);
	}
}

QSize DisplayQt::contentSize() const {
	unsigned w, h;
	VideoBackendGetFrameSize(&m_backend, &w, &h);
	return {saturateCast<int>(w), saturateCast<int>(h)};
}

void DisplayQt::init(struct VideoBackend*, WHandle) {
}

void DisplayQt::deinit(struct VideoBackend*) {
}

void DisplayQt::setLayerDimensions(struct VideoBackend* v, enum VideoLayer layer, const struct mRectangle* dims) {
	DisplayQt* self = static_cast<DisplayQt*>(v->user);
	if (layer > self->m_layerDims.size()) {
		return;
	}
	self->m_layerDims[layer] = QRect(dims->x, dims->y, dims->width, dims->height);
}

void DisplayQt::layerDimensions(const struct VideoBackend* v, enum VideoLayer layer, struct mRectangle* dims) {
	DisplayQt* self = static_cast<DisplayQt*>(v->user);
	if (layer > self->m_layerDims.size()) {
		return;
	}
	QRect rect = self->m_layerDims[layer];
	dims->x = rect.x();
	dims->y = rect.y();
	dims->width = rect.width();
	dims->height = rect.height();
}

void DisplayQt::swap(struct VideoBackend*) {
}

void DisplayQt::clear(struct VideoBackend*) {
}

void DisplayQt::contextResized(struct VideoBackend*, unsigned, unsigned) {
}

void DisplayQt::setImageSize(struct VideoBackend* v, enum VideoLayer layer, int w, int h) {
	DisplayQt* self = static_cast<DisplayQt*>(v->user);
	if (layer > self->m_layers.size()) {
		return;
	}
	self->m_layers[layer] = QImage(w, h, QImage::Format_ARGB32);
}

void DisplayQt::imageSize(struct VideoBackend* v, enum VideoLayer layer, int* w, int* h) {
	DisplayQt* self = static_cast<DisplayQt*>(v->user);
	if (layer > self->m_layers.size()) {
		return;
	}
	*w = self->m_layers[layer].width();
	*h = self->m_layers[layer].height();
}

void DisplayQt::setImage(struct VideoBackend* v, enum VideoLayer layer, const void* frame) {
	DisplayQt* self = static_cast<DisplayQt*>(v->user);
	if (layer > self->m_layers.size()) {
		return;
	}
	QImage& image = self->m_layers[layer];
	self->m_layers[layer] = QImage(static_cast<const uchar*>(frame), image.width(), image.height(), QImage::Format_ARGB32).rgbSwapped();
}

void DisplayQt::drawFrame(struct VideoBackend* v) {
	QMetaObject::invokeMethod(static_cast<DisplayQt*>(v->user), "update");
}
