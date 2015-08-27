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
#include <3ds/svc.h>

extern char* fake_heap_start;
extern char* fake_heap_end;
u32 __linear_heap;
u32 __heapBase;
static u32 __heap_size = 0x03000000;
static u32 __linear_heap_size = 0x00800000;

extern void (*__system_retAddr)(void);

void __destroy_handle_list(void);
void __appExit();

void __libc_fini_array(void);

uint32_t* romBuffer;
size_t romBufferSize;

bool allocateRomBuffer(void) {
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

	// Allocate the application heap
	__heapBase = 0x08000000;
	svcControlMemory(&tmp, __heapBase, 0x0, __heap_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);

	// Allocate the linear heap
	svcControlMemory(&__linear_heap, 0x0, 0x0, __linear_heap_size, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);
	// Set up newlib heap
	fake_heap_start = (char*)__heapBase;
	fake_heap_end = fake_heap_start + __heap_size;
}

void __attribute__((noreturn)) __libctru_exit(int rc)
{
	u32 tmp=0;

	// Unmap the linear heap
	svcControlMemory(&tmp, __linear_heap, 0x0, __linear_heap_size, MEMOP_FREE, 0x0);

	// Unmap the application heap
	svcControlMemory(&tmp, __heapBase, 0x0, __heap_size, MEMOP_FREE, 0x0);

	// Close some handles
	__destroy_handle_list();

	// Jump to the loader if it provided a callback
	if (__system_retAddr)
		__system_retAddr();

	// Since above did not jump, end this process
	svcExitProcess();
}
