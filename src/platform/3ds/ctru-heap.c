/* This code is mostly from ctrulib, which contains the following license:

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	* The origin of this software must not be misrepresented; you must not
	  claim that you wrote the original software. If you use this software in
	  a product, an acknowledgment in the product documentation would be
	  appreciated but is not required.

	* Altered source versions must be plainly marked as such, and must not be
	  misrepresented as being the original software.

	* This notice may not be removed or altered from any source distribution.
*/

#include <3ds/types.h>
#include <3ds/srv.h>
#include <3ds/gfx.h>
#include <3ds/sdmc.h>
#include <3ds/services/apt.h>
#include <3ds/services/fs.h>
#include <3ds/services/hid.h>
#include <3ds/svc.h>

#include <mgba-util/common.h>

extern char* fake_heap_start;
extern char* fake_heap_end;
extern u32 __ctru_linear_heap;
extern u32 __ctru_heap;
extern u32 __ctru_heap_size;
extern u32 __ctru_linear_heap_size;
static u32 __custom_heap_size = 0x02400000;
static u32 __custom_linear_heap_size = 0x01400000;

uint32_t* romBuffer = NULL;
size_t romBufferSize;

FS_Archive sdmcArchive;

bool allocateRomBuffer(void) {
	if (romBuffer) {
		return true;
	}
	romBuffer = malloc(0x02000000);
	if (romBuffer) {
		romBufferSize = 0x02000000;
		return true;
	}
	romBuffer = malloc(0x01000000);
	if (romBuffer) {
		romBufferSize = 0x01000000;
		return true;
	}
	return false;
}

void __system_allocateHeaps() {
	u32 tmp=0;

	__ctru_heap_size = __custom_heap_size;
	__ctru_linear_heap_size = __custom_linear_heap_size;

	// Allocate the application heap
	__ctru_heap = 0x08000000;
	svcControlMemory(&tmp, __ctru_heap, 0x0, __ctru_heap_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);

	// Allocate the linear heap
	svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, __ctru_linear_heap_size, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
	// Set up newlib heap
	fake_heap_start = (char*)__ctru_heap;
	fake_heap_end = fake_heap_start + __ctru_heap_size;
}

void __appInit(void) {
	// Initialize services
	srvInit();
	aptInit();
	hidInit();

	fsInit();
	sdmcInit();

	FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
	allocateRomBuffer();
}
