/*
 mgba.h
 Copyright (c) 2016 Jeffrey Pfau

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sys/iosupport.h>
#include <gba_types.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <mgba.h>

#define REG_DEBUG_ENABLE (vu16*) 0x4FFF780
#define REG_DEBUG_FLAGS (vu16*) 0x4FFF700
#define REG_DEBUG_STRING (char*) 0x4FFF600

ssize_t mgba_stdout_write(struct _reent* r __attribute__((unused)), void* fd __attribute__((unused)), const char* ptr, size_t len) {
	if (len > 0x100) {
		len = 0x100;
	}
	strncpy(REG_DEBUG_STRING, ptr, len);
	*REG_DEBUG_FLAGS = MGBA_LOG_INFO | 0x100;
	return len;
}

ssize_t mgba_stderr_write(struct _reent* r __attribute__((unused)), void* fd __attribute__((unused)), const char* ptr, size_t len) {
	if (len > 0x100) {
		len = 0x100;
	}
	strncpy(REG_DEBUG_STRING, ptr, len);
	*REG_DEBUG_FLAGS = MGBA_LOG_ERROR | 0x100;
	return len;
}

void mgba_printf(int level, const char* ptr, ...) {
	level &= 0x7;
	va_list args;
	va_start(args, ptr);
	vsnprintf(REG_DEBUG_STRING, 0x100, ptr, args);
	va_end(args);
	*REG_DEBUG_FLAGS = level | 0x100;
}

static const devoptab_t dotab_mgba_stdout = {
	"mgba_stdout",
	0,
	NULL,
	NULL,
	mgba_stdout_write
};

static const devoptab_t dotab_mgba_stderr = {
	"mgba_stderr",
	0,
	NULL,
	NULL,
	mgba_stderr_write
};

bool mgba_console_open(void) {
	if (!mgba_open()) {
		return false;
	}
	devoptab_list[STD_OUT] = &dotab_mgba_stdout;
	devoptab_list[STD_ERR] = &dotab_mgba_stderr;
	return true;
}

bool mgba_open(void) {
	*REG_DEBUG_ENABLE = 0xC0DE;
	return *REG_DEBUG_ENABLE == 0x1DEA;
}

void mgba_close(void) {
	*REG_DEBUG_ENABLE = 0;
}
