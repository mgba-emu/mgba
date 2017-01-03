/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/text-codec.h>
#include <mgba-util/vfs.h>

int main(int argc, char** argv) {
	struct TextCodec codec;
	struct VFile* vf = VFileOpen(argv[1], O_RDONLY);
	TextCodecLoadTBL(&codec, vf, true);
	vf->close(vf);

	vf = VFileOpen(argv[2], O_RDONLY);
	struct TextCodecIterator iter;
	TextCodecStartDecode(&codec, &iter);
	uint8_t lineBuffer[128];
	uint8_t c;
	while (vf->read(vf, &c, 1) > 0) {
		TextCodecAdvance(&iter, c, lineBuffer, sizeof(lineBuffer));
	}
	TextCodecFinish(&iter, lineBuffer, sizeof(lineBuffer));

	TextCodecDeinit(&codec);
	return 0;
}

