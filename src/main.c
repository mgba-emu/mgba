#include "arm.h"
#include "gba.h"
#include "isa-arm.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char** argv) {
	struct GBA gba;
	GBAInit(&gba);
	int fd = open("test.rom", O_RDONLY);
	GBALoadROM(&gba, fd);
	gba.cpu.gprs[ARM_PC] = 0x08000004;
	gba.memory.d.setActiveRegion(&gba.memory.d, gba.cpu.gprs[ARM_PC]);
	int i;
	for (i = 0; i < 1024 * 1024 * 16; ++i) {
		ARMRun(&gba.cpu);
	}
	GBADeinit(&gba);
	close(fd);

	return 0;
}
