/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "video-backend.h"

void VideoBackendGetFrameSize(const struct VideoBackend* v, unsigned* width, unsigned* height) {
	*width = 0;
	*height = 0;
	int i;
	for (i = 0; i < VIDEO_LAYER_MAX; ++i) {
		struct Rectangle dims;
		v->layerDimensions(v, i, &dims);
		if (dims.x + dims.width > *width) {
			*width = dims.x + dims.width;
		}
		if (dims.y + dims.height > *height) {
			*height = dims.y + dims.height;
		}
	}
}
