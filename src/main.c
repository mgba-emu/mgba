#include "gba.h"

int main(int argc, char** argv) {
	struct GBA gba;
	GBAInit(&gba);
	GBADeinit(&gba);

	return 0;
}