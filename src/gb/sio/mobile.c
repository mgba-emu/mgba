#ifdef USE_LIBMOBILE

#include <mgba/internal/gb/sio/mobile.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>

mLOG_DECLARE_CATEGORY(GB_MOBILE);
mLOG_DEFINE_CATEGORY(GB_MOBILE, "Mobile Adapter (GBC)", "gb.mobile");

static void debug_log(void* user, const char* line) {
	UNUSED(user);
	mLOG(GB_MOBILE, DEBUG, "%s", line);
}

static void time_latch(void* user, unsigned timer) {
	struct GBMobileAdapter* adapter = ((struct MobileAdapterGB*) user)->p;

	adapter->m.timeLatch[timer] = (unsigned) mTimingCurrentTime(&adapter->d.p->p->timing);
}

static bool time_check_ms(void* user, unsigned timer, unsigned ms) {
	struct GBMobileAdapter* adapter = ((struct MobileAdapterGB*) user)->p;

	unsigned diff = (unsigned) mTimingCurrentTime(&adapter->d.p->p->timing) - (unsigned) adapter->m.timeLatch[timer];
	return (unsigned) ((uint64_t) diff * 1000ULL / (uint64_t) CGB_SM83_FREQUENCY) >= ms;
}

static bool GBMobileAdapterInit(struct GBSIODriver* driver);
static void GBMobileAdapterDeinit(struct GBSIODriver* driver);
static void GBMobileAdapterWriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t GBMobileAdapterWriteSC(struct GBSIODriver* driver, uint8_t value);

void GBMobileAdapterCreate(struct GBMobileAdapter* mobile) {
	mobile->d.init = GBMobileAdapterInit;
	mobile->d.deinit = GBMobileAdapterDeinit;
	mobile->d.writeSB = GBMobileAdapterWriteSB;
	mobile->d.writeSC = GBMobileAdapterWriteSC;

	memset(&mobile->m, 0, sizeof(mobile->m));
	mobile->m.p = mobile;
}

bool GBMobileAdapterInit(struct GBSIODriver* driver) {
	struct GBMobileAdapter* mobile = (struct GBMobileAdapter*) driver;
	mobile->m.adapter = mobile_new(&mobile->m);
	if (!mobile->m.adapter) {
		return false;
	}
#define _MOBILE_SETCB(NAME) mobile_def_ ## NAME(mobile->m.adapter, NAME);
	_MOBILE_SETCB(debug_log);
	_MOBILE_SETCB(serial_disable);
	_MOBILE_SETCB(serial_enable);
	_MOBILE_SETCB(config_read);
	_MOBILE_SETCB(config_write);
	_MOBILE_SETCB(time_latch);
	_MOBILE_SETCB(time_check_ms);
	_MOBILE_SETCB(sock_open);
	_MOBILE_SETCB(sock_close);
	_MOBILE_SETCB(sock_connect);
	_MOBILE_SETCB(sock_listen);
	_MOBILE_SETCB(sock_accept);
	_MOBILE_SETCB(sock_send);
	_MOBILE_SETCB(sock_recv);
	_MOBILE_SETCB(update_number);
#undef _MOBILE_SETCB
	mobile_start(mobile->m.adapter);
	return true;
}

void GBMobileAdapterDeinit(struct GBSIODriver* driver) {
	struct GBMobileAdapter* mobile = (struct GBMobileAdapter*) driver;
	mobile_config_save(mobile->m.adapter);
	mobile_stop(mobile->m.adapter);
}

void GBMobileAdapterWriteSB(struct GBSIODriver* driver, uint8_t value) {
	(*(struct GBMobileAdapter*) driver).nextData[0] = value;
}

uint8_t GBMobileAdapterWriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct GBMobileAdapter* mobile = (struct GBMobileAdapter*) driver;
	if ((value & 0x81) == 0x81) {
		mobile_loop(mobile->m.adapter);
		if (mobile->m.serial == 1) {
			driver->p->pendingSB = mobile->nextData[1];
			mobile->nextData[1] = mobile_transfer(mobile->m.adapter, mobile->nextData[0]);
		}
	}
	return value;
}

#endif
