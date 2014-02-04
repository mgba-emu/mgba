#ifndef GBA_SIO_H
#define GBA_SIO_H

#include <stdint.h>

enum GBASIOMode {
	SIO_NORMAL_8 = 0,
	SIO_NORMAL_32 = 1,
	SIO_MULTI = 2,
	SIO_UART = 3,
	SIO_GPIO = 8,
	SIO_JOYBUS = 12
};

enum GBASIOMultiMode {
	VBA_LINK_COMPAT
};

enum {
	RCNT_INITIAL = 0x8000
};

struct GBASIODriver {
	int (*attach)(struct GBASIODriver* driver);
};

struct GBASIO {
	struct GBA* p;

	enum GBASIOMode mode;
	enum GBASIOMultiMode multiMode;
	struct GBASIODriver* driver;

	uint16_t rcnt;
	union {
		struct {
			unsigned sc : 1;
			unsigned internalSc : 1;
			unsigned si : 1;
			unsigned idleSo : 1;
			unsigned : 4;
			unsigned start : 1;
			unsigned : 3;
			unsigned length : 1;
			unsigned : 1;
			unsigned irq : 1;
			unsigned : 1;
		} normalControl;

		struct {
			unsigned baud : 2;
			unsigned slave : 1;
			unsigned ready : 1;
			unsigned id : 2;
			unsigned error : 1;
			unsigned busy : 1;
			unsigned : 6;
			unsigned irq : 1;
			unsigned : 1;
		} multiplayerControl;

		uint16_t siocnt;
	};
};

void GBASIOInit(struct GBASIO* sio);

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value);
void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value);
void GBASIOWriteSIOMLT_SEND(struct GBASIO* sio, uint16_t value);

#endif
