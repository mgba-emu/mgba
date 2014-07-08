#include "gba-serialize.h"

#include "gba-audio.h"
#include "gba-io.h"
#include "gba-thread.h"

#include "util/memory.h"

#include <fcntl.h>

const uint32_t GBA_SAVESTATE_MAGIC = 0x01000000;

void GBASerialize(struct GBA* gba, struct GBASerializedState* state) {
	state->versionMagic = GBA_SAVESTATE_MAGIC;
	state->biosChecksum = gba->biosChecksum;
	state->id = ((struct GBACartridge*) gba->memory.rom)->id;
	memcpy(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title));

	memcpy(state->cpu.gprs, gba->cpu->gprs, sizeof(state->cpu.gprs));
	state->cpu.cpsr = gba->cpu->cpsr;
	state->cpu.spsr = gba->cpu->spsr;
	state->cpu.cycles = gba->cpu->cycles;
	state->cpu.nextEvent = gba->cpu->nextEvent;
	memcpy(state->cpu.bankedRegisters, gba->cpu->bankedRegisters, 6 * 7 * sizeof(int32_t));
	memcpy(state->cpu.bankedSPSRs, gba->cpu->bankedSPSRs, 6 * sizeof(int32_t));

	GBAMemorySerialize(&gba->memory, state);
	GBAIOSerialize(gba, state);
	GBAVideoSerialize(&gba->video, state);
	GBAAudioSerialize(&gba->audio, state);
}

void GBADeserialize(struct GBA* gba, struct GBASerializedState* state) {
	if (state->versionMagic != GBA_SAVESTATE_MAGIC) {
		GBALog(gba, GBA_LOG_WARN, "Invalid or too new savestate");
		return;
	}
	if (state->biosChecksum != gba->biosChecksum) {
		GBALog(gba, GBA_LOG_WARN, "Savestate created using a different version of the BIOS");
		if (state->cpu.gprs[ARM_PC] < SIZE_BIOS && state->cpu.gprs[ARM_PC] >= 0x20) {
			return;
		}
	}
	if (state->id != ((struct GBACartridge*) gba->memory.rom)->id || memcmp(state->title, ((struct GBACartridge*) gba->memory.rom)->title, sizeof(state->title))) {
		GBALog(gba, GBA_LOG_WARN, "Savestate is for a different game");
		return;
	}
	memcpy(gba->cpu->gprs, state->cpu.gprs, sizeof(gba->cpu->gprs));
	gba->cpu->cpsr = state->cpu.cpsr;
	gba->cpu->spsr = state->cpu.spsr;
	gba->cpu->cycles = state->cpu.cycles;
	gba->cpu->nextEvent = state->cpu.nextEvent;
	memcpy(gba->cpu->bankedRegisters, state->cpu.bankedRegisters, 6 * 7 * sizeof(int32_t));
	memcpy(gba->cpu->bankedSPSRs, state->cpu.bankedSPSRs, 6 * sizeof(int32_t));
	gba->cpu->executionMode = gba->cpu->cpsr.t ? MODE_THUMB : MODE_ARM;
	gba->cpu->privilegeMode = gba->cpu->cpsr.priv;
	gba->cpu->memory.setActiveRegion(gba->cpu, gba->cpu->gprs[ARM_PC]);

	GBAMemoryDeserialize(&gba->memory, state);
	GBAIODeserialize(gba, state);
	GBAVideoDeserialize(&gba->video, state);
	GBAAudioDeserialize(&gba->audio, state);
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

struct GBASerializedState* GBAAllocateState(void) {
	return anonymousMemoryMap(sizeof(struct GBASerializedState));
}

void GBADeallocateState(struct GBASerializedState* state) {
	mappedMemoryFree(state, sizeof(struct GBASerializedState));
}

void GBARecordFrame(struct GBAThread* thread) {
	int offset = thread->rewindBufferWriteOffset;
	struct GBASerializedState* state = thread->rewindBuffer[offset];
	if (!state) {
		state = GBAAllocateState();
		thread->rewindBuffer[offset] = state;
	}
	GBASerialize(thread->gba, state);
	thread->rewindBufferSize = thread->rewindBufferSize == thread->rewindBufferCapacity ? thread->rewindBufferCapacity : thread->rewindBufferSize + 1;
	thread->rewindBufferWriteOffset = (offset + 1) % thread->rewindBufferCapacity;
}

void GBARewind(struct GBAThread* thread, int nStates) {
	if (nStates > thread->rewindBufferSize || nStates < 0) {
		nStates = thread->rewindBufferSize;
	}
	if (nStates == 0) {
		return;
	}
	int offset = thread->rewindBufferWriteOffset - nStates;
	if (offset < 0) {
		offset += thread->rewindBufferSize;
	}
	struct GBASerializedState* state = thread->rewindBuffer[offset];
	if (!state) {
		return;
	}
	thread->rewindBufferSize -= nStates;
	thread->rewindBufferWriteOffset = (offset + thread->rewindBufferCapacity - nStates) % thread->rewindBufferCapacity;
	GBADeserialize(thread->gba, state);
}
