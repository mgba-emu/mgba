/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Display.h"

#include <QImage>
#include <QTimer>

#include <mgba/feature/video-backend.h>
#include <array>

namespace QGBA {

class DisplayQt : public Display {
Q_OBJECT

public:
	DisplayQt(QWidget* parent = nullptr);

	void startDrawing(std::shared_ptr<CoreController>) override;
	bool isDrawing() const override { return m_isDrawing; }
	bool supportsShaders() const override { return false; }
	VideoShader* shaders() override { return nullptr; }
	QSize contentSize() const override;
	VideoBackend* videoBackend() override { return &m_backend; }

public slots:
	void stopDrawing() override;
	void pauseDrawing() override { m_isDrawing = false; }
	void unpauseDrawing() override { m_isDrawing = true; }
	void forceDraw() override { update(); }
	void lockAspectRatio(bool lock) override;
	void lockIntegerScaling(bool lock) override;
	void interframeBlending(bool enable) override;
	void swapInterval(int) override {};
	void filter(bool filter) override;
	void framePosted() override;
	void setShaders(struct VDir*) override {}
	void clearShaders() override {}
	void resizeContext() override;
	void setBackgroundImage(const QImage&) override;

protected:
	virtual void paintEvent(QPaintEvent*) override;

private:
	void redoBounds();

	static void init(struct VideoBackend*, WHandle);
	static void deinit(struct VideoBackend*);
	static void setLayerDimensions(struct VideoBackend*, enum VideoLayer, const struct mRectangle*);
	static void layerDimensions(const struct VideoBackend*, enum VideoLayer, struct mRectangle*);
	static void swap(struct VideoBackend*);
	static void clear(struct VideoBackend*);
	static void contextResized(struct VideoBackend*, unsigned w, unsigned h);
	static void setImageSize(struct VideoBackend*, enum VideoLayer, int w, int h);
	static void imageSize(struct VideoBackend*, enum VideoLayer, int* w, int* h);
	static void setImage(struct VideoBackend*, enum VideoLayer, const void* frame);
	static void drawFrame(struct VideoBackend*);

	VideoBackend m_backend{};
	std::array<QRect, VIDEO_LAYER_MAX> m_layerDims;
	std::array<QImage, VIDEO_LAYER_MAX> m_layers;
	bool m_isDrawing = false;
	int m_width = -1;
	int m_height = -1;
	QImage m_oldBacking{nullptr};
	std::shared_ptr<CoreController> m_context = nullptr;
};

}
