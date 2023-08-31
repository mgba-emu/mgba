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

	adapter->timeLatch[timer] = mTimingCurrentTime(&adapter->d.p->p->timing);
}

static bool time_check_ms(void* user, unsigned timer, unsigned ms) {
	struct GBSIOMobileAdapter* adapter = ((struct MobileAdapterGB*) user)->p;

	uint32_t time = mTimingCurrentTime(&adapter->d.p->p->timing);
	uint32_t diff = time - adapter->timeLatch[timer];
	uint32_t cycles = (double) ms * CGB_SM83_FREQUENCY / 1000;
	return diff >= cycles;
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
	mobile->m.adapter = MobileAdapterGBNew(&mobile->m);
	if (!mobile->m.adapter) return false;

	mobile_def_debug_log(mobile->m.adapter, debug_log);
	mobile_def_time_latch(mobile->m.adapter, time_latch);
	mobile_def_time_check_ms(mobile->m.adapter, time_check_ms);

	mobile_start(mobile->m.adapter);
	return true;
}

void GBSIOMobileAdapterDeinit(struct GBSIODriver* driver) {
	struct GBSIOMobileAdapter* mobile = (struct GBSIOMobileAdapter*) driver;
	if (!mobile->m.adapter) return;

	mobile_config_save(mobile->m.adapter);
	mobile_stop(mobile->m.adapter);
	free(mobile->m.adapter);
	mobile->m.adapter = NULL;
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
