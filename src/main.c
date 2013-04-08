#include "arm.h"
#include "gba.h"
#include "isa-arm.h"

#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char** argv) {
	struct GBA gba;
	GBAInit(&gba);
	int fd = open("test.rom", O_RDONLY);
	GBALoadROM(&gba, fd);
	gba.cpu.gprs[ARM_PC] = 0x08000004;
	ARMStep(&gba.cpu);
	ARMStep(&gba.cpu);
	GBADeinit(&gba);

	return 0;
}
