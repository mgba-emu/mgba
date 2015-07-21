/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef INTERFACE_H
#define INTERFACE_H

#include "util/common.h"

enum GBALogLevel {
	GBA_LOG_FATAL = 0x01,
	GBA_LOG_ERROR = 0x02,
	GBA_LOG_WARN = 0x04,
	GBA_LOG_INFO = 0x08,
	GBA_LOG_DEBUG = 0x10,
	GBA_LOG_STUB = 0x20,

	GBA_LOG_GAME_ERROR = 0x100,
	GBA_LOG_SWI = 0x200,
	GBA_LOG_STATUS = 0x400,
	GBA_LOG_SIO = 0x800,

	GBA_LOG_ALL = 0xF3F,
};

enum GBAKey {
	GBA_KEY_A = 0,
	GBA_KEY_B = 1,
	GBA_KEY_SELECT = 2,
	GBA_KEY_START = 3,
	GBA_KEY_RIGHT = 4,
	GBA_KEY_LEFT = 5,
	GBA_KEY_UP = 6,
	GBA_KEY_DOWN = 7,
	GBA_KEY_R = 8,
	GBA_KEY_L = 9,
	GBA_KEY_MAX,
	GBA_KEY_NONE = -1
};

enum GBASIOMode {
	SIO_NORMAL_8 = 0,
	SIO_NORMAL_32 = 1,
	SIO_MULTI = 2,
	SIO_UART = 3,
	SIO_GPIO = 8,
	SIO_JOYBUS = 12
};

struct GBA;
struct GBAAudio;
struct GBASIO;
struct GBAThread;
struct GBAVideoRenderer;

typedef void (*GBALogHandler)(struct GBAThread*, enum GBALogLevel, const char* format, va_list args);

struct GBAAVStream {
	void (*postVideoFrame)(struct GBAAVStream*, struct GBAVideoRenderer* renderer);
	void (*postAudioFrame)(struct GBAAVStream*, int16_t left, int16_t right);
	void (*postAudioBuffer)(struct GBAAVStream*, struct GBAAudio*);
};

struct GBAKeyCallback {
	uint16_t (*readKeys)(struct GBAKeyCallback*);
};

struct GBAStopCallback {
	void (*stop)(struct GBAStopCallback*);
};

struct GBARotationSource {
	void (*sample)(struct GBARotationSource*);

	int32_t (*readTiltX)(struct GBARotationSource*);
	int32_t (*readTiltY)(struct GBARotationSource*);

	int32_t (*readGyroZ)(struct GBARotationSource*);
};

struct GBALuminanceSource {
	void (*sample)(struct GBALuminanceSource*);

	uint8_t (*readLuminance)(struct GBALuminanceSource*);
};

/* Have to include this here because of time_t returntype below. */
#include <time.h> 

struct GBARTCSource {
	void (*sample)(struct GBARTCSource*);

	time_t (*unixTime)(struct GBARTCSource*);
};

struct GBASIODriver {
	struct GBASIO* p;

	bool (*init)(struct GBASIODriver* driver);
	void (*deinit)(struct GBASIODriver* driver);
	bool (*load)(struct GBASIODriver* driver);
	bool (*unload)(struct GBASIODriver* driver);
	uint16_t (*writeRegister)(struct GBASIODriver* driver, uint32_t address, uint16_t value);
	int32_t (*processEvents)(struct GBASIODriver* driver, int32_t cycles);
};

#endif
