#include "gba-sio.h"

#include "gba-io.h"

static void _switchMode(struct GBASIO* sio) {
	int mode = ((sio->rcnt >> 14) & 0xC) | ((sio->siocnt >> 12) & 0x3);
	if (mode < 8) {
		sio->mode = (enum GBASIOMode) (mode & 0x3);
	} else {
		sio->mode = (enum GBASIOMode) (mode & 0xC);
	}
	// TODO: hangup if we have an existing connection
}

void GBASIOInit(struct GBASIO* sio) {
	sio->rcnt = RCNT_INITIAL;
	sio->siocnt = 0;
	_switchMode(sio);
}

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value) {
	sio->rcnt = value;
	_switchMode(sio);
}

void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value) {
	sio->siocnt = value;
	_switchMode(sio);
}
