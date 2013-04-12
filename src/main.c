#include "arm.h"
#include "debugger.h"
#include "gba.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv) {
	struct ARMDebugger debugger;
	struct GBA gba;
	GBAInit(&gba);
	int fd = open("test.rom", O_RDONLY);
	GBALoadROM(&gba, fd);
	gba.cpu.gprs[ARM_PC] = 0x08000004;
	gba.memory.d.setActiveRegion(&gba.memory.d, gba.cpu.gprs[ARM_PC]);
	ARMDebuggerInit(&debugger, &gba.cpu);
	ARMDebuggerEnter(&debugger);
	GBADeinit(&gba);
	close(fd);

	return 0;
}
