#ifndef GBA_SENSORS_H
#define GBA_SENSORS_H

#include "common.h"

struct GBARotationSource {
	void (*sample)(struct GBARotationSource*);

	int32_t (*readTiltX)(struct GBARotationSource*);
	int32_t (*readTiltY)(struct GBARotationSource*);

	int32_t (*readGyroZ)(struct GBARotationSource*);
};

#endif
