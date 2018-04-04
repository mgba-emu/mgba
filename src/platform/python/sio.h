/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef M_CORE_GBA

#include <mgba/gba/interface.h>

struct GBASIOPythonDriver {
    struct GBASIODriver d;
    void* pyobj;
};

struct GBASIODriver* GBASIOPythonDriverCreate(void* pyobj);
struct GBASIODriver* GBASIOJOYPythonDriverCreate(void* pyobj);

PYEXPORT bool _pyGBASIOPythonDriverInit(void* driver);
PYEXPORT void _pyGBASIOPythonDriverDeinit(void* driver);
PYEXPORT bool _pyGBASIOPythonDriverLoad(void* driver);
PYEXPORT bool _pyGBASIOPythonDriverUnload(void* driver);
PYEXPORT uint16_t _pyGBASIOPythonDriverWriteRegister(void* driver, uint32_t address, uint16_t value);

#endif

#ifdef M_CORE_GB

#include <mgba/gb/interface.h>

struct GBSIOPythonDriver {
    struct GBSIODriver d;
    void* pyobj;
};

struct GBSIODriver* GBSIOPythonDriverCreate(void* pyobj);

PYEXPORT bool _pyGBSIOPythonDriverInit(void* driver);
PYEXPORT void _pyGBSIOPythonDriverDeinit(void* driver);
PYEXPORT void _pyGBSIOPythonDriverWriteSB(void* driver, uint8_t value);
PYEXPORT uint8_t _pyGBSIOPythonDriverWriteSC(void* driver, uint8_t value);

#endif
