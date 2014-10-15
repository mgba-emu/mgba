#ifndef CRC32_H
#define CRC32_H

#include "util/common.h"

struct VFile;

uint32_t doCrc32(const void* buf, size_t size);
uint32_t updateCrc32(uint32_t crc, const void* buf, size_t size);
uint32_t fileCrc32(struct VFile* file, size_t endOffset);

#endif
