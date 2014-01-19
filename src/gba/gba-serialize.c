#include "gba-serialize.h"

#include "memory.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const uint32_t GBA_SAVESTATE_MAGIC = 0x01000000;

void GBASerialize(struct GBA* gba, struct GBASerializedState* state) {
	memcpy(state->cpu.gprs, gba->cpu.gprs, sizeof(state->cpu.gprs));
	state->cpu.cpsr = gba->cpu.cpsr;
	state->cpu.spsr = gba->cpu.spsr;
	state->cpu.cycles = gba->cpu.cycles;
	state->cpu.nextEvent = gba->cpu.nextEvent;
	memcpy(state->cpu.bankedRegisters, gba->cpu.bankedRegisters, 6 * 7 * sizeof(int32_t));
	memcpy(state->cpu.bankedSPSRs, gba->cpu.bankedSPSRs, 6 * sizeof(int32_t));

	GBAMemorySerialize(&gba->memory, state);
}

void GBADeserialize(struct GBA* gba, struct GBASerializedState* state) {
	memcpy(gba->cpu.gprs, state->cpu.gprs, sizeof(gba->cpu.gprs));
	gba->cpu.cpsr = state->cpu.cpsr;
	gba->cpu.spsr = state->cpu.spsr;
	gba->cpu.cycles = state->cpu.cycles;
	gba->cpu.nextEvent = state->cpu.nextEvent;
	memcpy(gba->cpu.bankedRegisters, state->cpu.bankedRegisters, 6 * 7 * sizeof(int32_t));
	memcpy(gba->cpu.bankedSPSRs, state->cpu.bankedSPSRs, 6 * sizeof(int32_t));
	gba->cpu.executionMode = gba->cpu.cpsr.t ? MODE_THUMB : MODE_ARM;
	ARMSetPrivilegeMode(&gba->cpu, gba->cpu.cpsr.priv);
	gba->cpu.memory->setActiveRegion(gba->cpu.memory, gba->cpu.gprs[ARM_PC]);

	GBAMemoryDeserialize(&gba->memory, state);
}

static int _getStateFd(struct GBA* gba, int slot) {
	char path[PATH_MAX];
	path[PATH_MAX - 1] = '\0';
	snprintf(path, PATH_MAX - 1, "%s.ss%d", gba->activeFile, slot);
	int fd = open(path, O_CREAT | O_RDWR, 0777);
	if (fd >= 0) {
		ftruncate(fd, sizeof(struct GBASerializedState));
	}
	return fd;
}

int GBASaveState(struct GBA* gba, int slot) {
	int fd = _getStateFd(gba, slot);
	if (fd < 0) {
		return 0;
	}
	struct GBASerializedState* state = GBAMapState(fd);
	GBASerialize(gba, state);
	GBADeallocateState(state);
	close(fd);
	return 1;
}

int GBALoadState(struct GBA* gba, int slot) {
	int fd = _getStateFd(gba, slot);
	if (fd < 0) {
		return 0;
	}
	struct GBASerializedState* state = GBAMapState(fd);
	GBADeserialize(gba, state);
	GBADeallocateState(state);
	close(fd);
	return 1;
}

struct GBASerializedState* GBAMapState(int fd) {
	return fileMemoryMap(fd, sizeof(struct GBASerializedState), MEMORY_WRITE);
}

struct GBASerializedState* GBAAloocateState(void) {
	return anonymousMemoryMap(sizeof(struct GBASerializedState));
}

void GBADeallocateState(struct GBASerializedState* state) {
	mappedMemoryFree(state, sizeof(struct GBASerializedState));
}
