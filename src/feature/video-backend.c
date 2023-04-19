/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/feature/video-backend.h>

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
