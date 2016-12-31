/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mgba-util/vfs.h>

struct VFilePy {
	struct VFile d;
	void* fileobj;
};

struct VFile* VFileFromPython(void* fileobj);

extern "Python+C" {

bool _vfpClose(struct VFile* vf);
off_t _vfpSeek(struct VFile* vf, off_t offset, int whence);
ssize_t _vfpRead(struct VFile* vf, void* buffer, size_t size);
ssize_t _vfpWrite(struct VFile* vf, const void* buffer, size_t size);
void* _vfpMap(struct VFile* vf, size_t size, int flags);
void _vfpUnmap(struct VFile* vf, void* memory, size_t size);
void _vfpTruncate(struct VFile* vf, size_t size);
ssize_t _vfpSize(struct VFile* vf);
bool _vfpSync(struct VFile* vf, const void* buffer, size_t size);

}
