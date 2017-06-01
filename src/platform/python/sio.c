/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/gba/interface.h>

#include "flags.h"

#ifdef M_CORE_GBA

#define CREATE_SHIM(NAME, RETURN) \
	RETURN _pyGBASIOPythonDriver ## NAME (void* driver); \
	static RETURN _pyGBASIOPythonDriver ## NAME ## Shim(struct GBASIODriver* driver) { \
		struct GBASIODriver* py = (struct GBASIODriver*) driver; \
		return _pyGBASIOPythonDriver ## NAME(py); \
	}

#define CREATE_SHIM_ARGS(NAME, RETURN, TYPES, ...) \
	RETURN _pyGBASIOPythonDriver ## NAME TYPES; \
	static RETURN _pyGBASIOPythonDriver ## NAME ## Shim TYPES { \
		struct GBASIODriver* py = (struct GBASIODriver*) driver; \
		return _pyGBASIOPythonDriver ## NAME(py, __VA_ARGS__); \
	}

struct GBASIOPythonDriver {
    struct GBASIODriver d;
    void* pyobj;
};

CREATE_SHIM(Init, bool);
CREATE_SHIM(Deinit, void);
CREATE_SHIM(Load, bool);
CREATE_SHIM(Unload, bool);
CREATE_SHIM_ARGS(WriteRegister, uint16_t, (struct GBASIODriver* driver, uint32_t address, uint16_t value), address, value);

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

#endif
