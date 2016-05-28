/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "serialize.h"

#include "util/vfs.h"

struct mStateExtdataHeader {
	uint32_t tag;
	int32_t size;
	int64_t offset;
};

bool mStateExtdataInit(struct mStateExtdata* extdata) {
	memset(extdata->data, 0, sizeof(extdata->data));
	return true;
}

void mStateExtdataDeinit(struct mStateExtdata* extdata) {
	size_t i;
	for (i = 1; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data && extdata->data[i].clean) {
			extdata->data[i].clean(extdata->data[i].data);
		}
	}
}

void mStateExtdataPut(struct mStateExtdata* extdata, enum mStateExtdataTag tag, struct mStateExtdataItem* item) {
	if (tag == EXTDATA_NONE || tag >= EXTDATA_MAX) {
		return;
	}

	if (extdata->data[tag].data && extdata->data[tag].clean) {
		extdata->data[tag].clean(extdata->data[tag].data);
	}
	extdata->data[tag] = *item;
}

bool mStateExtdataGet(struct mStateExtdata* extdata, enum mStateExtdataTag tag, struct mStateExtdataItem* item) {
	if (tag == EXTDATA_NONE || tag >= EXTDATA_MAX) {
		return false;
	}

	*item = extdata->data[tag];
	return true;
}

bool mStateExtdataSerialize(struct mStateExtdata* extdata, struct VFile* vf) {
	ssize_t position = vf->seek(vf, 0, SEEK_CUR);
	ssize_t size = sizeof(struct mStateExtdataHeader);
	size_t i = 0;
	for (i = 1; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data) {
			size += sizeof(struct mStateExtdataHeader);
		}
	}
	if (size == sizeof(struct mStateExtdataHeader)) {
		return true;
	}
	struct mStateExtdataHeader* header = malloc(size);
	position += size;

	size_t j;
	for (i = 1, j = 0; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data) {
			STORE_32LE(i, offsetof(struct mStateExtdataHeader, tag), &header[j]);
			STORE_32LE(extdata->data[i].size, offsetof(struct mStateExtdataHeader, size), &header[j]);
			STORE_64LE(position, offsetof(struct mStateExtdataHeader, offset), &header[j]);
			position += extdata->data[i].size;
			++j;
		}
	}
	header[j].tag = 0;
	header[j].size = 0;
	header[j].offset = 0;

	if (vf->write(vf, header, size) != size) {
		free(header);
		return false;
	}
	free(header);

	for (i = 1; i < EXTDATA_MAX; ++i) {
		if (extdata->data[i].data) {
			if (vf->write(vf, extdata->data[i].data, extdata->data[i].size) != extdata->data[i].size) {
				return false;
			}
		}
	}
	return true;
}

bool mStateExtdataDeserialize(struct mStateExtdata* extdata, struct VFile* vf) {
	while (true) {
		struct mStateExtdataHeader buffer, header;
		if (vf->read(vf, &buffer, sizeof(buffer)) != sizeof(buffer)) {
			return false;
		}
		LOAD_32LE(header.tag, 0, &buffer.tag);
		LOAD_32LE(header.size, 0, &buffer.size);
		LOAD_64LE(header.offset, 0, &buffer.offset);

		if (header.tag == EXTDATA_NONE) {
			break;
		}
		if (header.tag >= EXTDATA_MAX) {
			continue;
		}
		ssize_t position = vf->seek(vf, 0, SEEK_CUR);
		if (vf->seek(vf, header.offset, SEEK_SET) < 0) {
			return false;
		}
		struct mStateExtdataItem item = {
			.data = malloc(header.size),
			.size = header.size,
			.clean = free
		};
		if (!item.data) {
			continue;
		}
		if (vf->read(vf, item.data, header.size) != header.size) {
			free(item.data);
			continue;
		}
		mStateExtdataPut(extdata, header.tag, &item);
		vf->seek(vf, position, SEEK_SET);
	};
	return true;
}
