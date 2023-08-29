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
	struct GBSIOMobileAdapter* adapter = ((struct MobileAdapterGB*) user)->p;

	adapter->m.timeLatch[timer] = (unsigned) mTimingCurrentTime(&adapter->d.p->p->timing);
}

static bool time_check_ms(void* user, unsigned timer, unsigned ms) {
	struct GBSIOMobileAdapter* adapter = ((struct MobileAdapterGB*) user)->p;

	unsigned diff = (unsigned) mTimingCurrentTime(&adapter->d.p->p->timing) - (unsigned) adapter->m.timeLatch[timer];
	return (unsigned) ((uint64_t) diff * 1000ULL / (uint64_t) CGB_SM83_FREQUENCY) >= ms;
}

static bool GBSIOMobileAdapterInit(struct GBSIODriver* driver);
static void GBSIOMobileAdapterDeinit(struct GBSIODriver* driver);
static void GBSIOMobileAdapterWriteSB(struct GBSIODriver* driver, uint8_t value);
static uint8_t GBSIOMobileAdapterWriteSC(struct GBSIODriver* driver, uint8_t value);

void GBSIOMobileAdapterCreate(struct GBSIOMobileAdapter* mobile) {
	mobile->d.init = GBSIOMobileAdapterInit;
	mobile->d.deinit = GBSIOMobileAdapterDeinit;
	mobile->d.writeSB = GBSIOMobileAdapterWriteSB;
	mobile->d.writeSC = GBSIOMobileAdapterWriteSC;

	memset(&mobile->m, 0, sizeof(mobile->m));
	mobile->m.p = mobile;
}

bool GBSIOMobileAdapterInit(struct GBSIODriver* driver) {
	struct GBSIOMobileAdapter* mobile = (struct GBSIOMobileAdapter*) driver;
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

void GBSIOMobileAdapterDeinit(struct GBSIODriver* driver) {
	struct GBSIOMobileAdapter* mobile = (struct GBSIOMobileAdapter*) driver;
	mobile_config_save(mobile->m.adapter);
	mobile_stop(mobile->m.adapter);
}

void GBSIOMobileAdapterWriteSB(struct GBSIODriver* driver, uint8_t value) {
	struct GBSIOMobileAdapter* mobile = (struct GBSIOMobileAdapter*) driver;
	mobile->byte = value;
}

uint8_t GBSIOMobileAdapterWriteSC(struct GBSIODriver* driver, uint8_t value) {
	struct GBSIOMobileAdapter* mobile = (struct GBSIOMobileAdapter*) driver;
	if ((value & 0x81) == 0x81) {
		if (mobile->m.serial == 1) {
			driver->p->pendingSB = mobile->next;
			mobile->next = mobile_transfer(mobile->m.adapter, mobile->byte);
		}
	}
	return value;
}

#endif
