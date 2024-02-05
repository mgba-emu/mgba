/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/feature/video-backend.h>

mLOG_DEFINE_CATEGORY(VIDEO, "Video backend", "video");

void VideoBackendGetFrame(const struct VideoBackend* v, struct mRectangle* frame) {
	memset(frame, 0, sizeof(*frame));
	int i;
	for (i = 0; i < VIDEO_LAYER_MAX; ++i) {
		struct mRectangle dims;
		v->layerDimensions(v, i, &dims);
		mRectangleUnion(frame, &dims);
	}
}

void VideoBackendGetFrameSize(const struct VideoBackend* v, unsigned* width, unsigned* height) {
	struct mRectangle frame;
	VideoBackendGetFrame(v, &frame);
	*width = frame.width;
	*height = frame.height;
}

void VideoBackendRecenter(struct VideoBackend* v, unsigned scale) {
	static const int centeredLayers[] = {VIDEO_LAYER_BACKGROUND, -1};
	struct mRectangle frame = {0};
	v->imageSize(v, VIDEO_LAYER_IMAGE, &frame.width, &frame.height);
	if (scale == 0) {
		scale = 1;
	}

	size_t i;
	for (i = 0; centeredLayers[i] >= 0; ++i) {
		int width, height;
		struct mRectangle dims = {0};
		v->imageSize(v, centeredLayers[i], &width, &height);
		dims.width = width * scale;
		dims.height = height * scale;
		mRectangleCenter(&frame, &dims);
		v->setLayerDimensions(v, centeredLayers[i], &dims);
	}
}
