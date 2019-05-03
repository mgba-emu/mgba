/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/flags.h>

#define CREATE_SHIM(PLAT, NAME, RETURN) \
	RETURN _py ## PLAT ## SIOPythonDriver ## NAME (void* driver); \
	static RETURN _py ## PLAT ## SIOPythonDriver ## NAME ## Shim(struct PLAT ## SIODriver* driver) { \
		struct PLAT ## SIODriver* py = (struct PLAT ## SIODriver*) driver; \
		return _py ## PLAT ## SIOPythonDriver ## NAME(py); \
	}

#define CREATE_SHIM_ARGS(PLAT, NAME, RETURN, TYPES, ...) \
	RETURN _py ## PLAT ## SIOPythonDriver ## NAME TYPES; \
	static RETURN _py ## PLAT ## SIOPythonDriver ## NAME ## Shim TYPES { \
		struct PLAT ## SIODriver* py = (struct PLAT ## SIODriver*) driver; \
		return _py ## PLAT ## SIOPythonDriver ## NAME(py, __VA_ARGS__); \
	}

#ifdef M_CORE_GBA

#include <mgba/gba/interface.h>

struct GBASIOPythonDriver {
    struct GBASIODriver d;
    void* pyobj;
};

CREATE_SHIM(GBA, Init, bool);
CREATE_SHIM(GBA, Deinit, void);
CREATE_SHIM(GBA, Load, bool);
CREATE_SHIM(GBA, Unload, bool);
CREATE_SHIM_ARGS(GBA, WriteRegister, uint16_t, (struct GBASIODriver* driver, uint32_t address, uint16_t value), address, value);

struct GBASIODriver* GBASIOPythonDriverCreate(void* pyobj) {
	struct GBASIOPythonDriver* driver = malloc(sizeof(*driver));
	driver->d.init = _pyGBASIOPythonDriverInitShim;
	driver->d.deinit = _pyGBASIOPythonDriverDeinitShim;
	driver->d.load = _pyGBASIOPythonDriverLoadShim;
	driver->d.unload = _pyGBASIOPythonDriverUnloadShim;
	driver->d.writeRegister = _pyGBASIOPythonDriverWriteRegisterShim;

	driver->pyobj = pyobj;
	return &driver->d;
}

struct GBASIODriver* GBASIOJOYPythonDriverCreate(void* pyobj) {
	struct GBASIOPythonDriver* driver = malloc(sizeof(*driver));
	GBASIOJOYCreate(&driver->d);
	driver->d.init = _pyGBASIOPythonDriverInitShim;
	driver->d.deinit = _pyGBASIOPythonDriverDeinitShim;
	driver->d.load = _pyGBASIOPythonDriverLoadShim;
	driver->d.unload = _pyGBASIOPythonDriverUnloadShim;

	driver->pyobj = pyobj;
	return &driver->d;
}

#endif

#ifdef M_CORE_GB

#include <mgba/gb/interface.h>

struct GBSIOPythonDriver {
    struct GBSIODriver d;
    void* pyobj;
};

CREATE_SHIM(GB, Init, bool);
CREATE_SHIM(GB, Deinit, void);
CREATE_SHIM_ARGS(GB, WriteSB, void, (struct GBSIODriver* driver, uint8_t value), value);
CREATE_SHIM_ARGS(GB, WriteSC, uint8_t, (struct GBSIODriver* driver, uint8_t value), value);

struct GBSIODriver* GBSIOPythonDriverCreate(void* pyobj) {
	struct GBSIOPythonDriver* driver = malloc(sizeof(*driver));
	driver->d.init = _pyGBSIOPythonDriverInitShim;
	driver->d.deinit = _pyGBSIOPythonDriverDeinitShim;
	driver->d.writeSB = _pyGBSIOPythonDriverWriteSBShim;
	driver->d.writeSC = _pyGBSIOPythonDriverWriteSCShim;

	driver->pyobj = pyobj;
	return &driver->d;
}

#endif
