#ifndef GBA_SERIALIZE_H
#define GBA_SERIALIZE_H

#include "gba.h"

const uint32_t GBA_SAVESTATE_MAGIC;

/* Savestate format:
 * 0x00000 - 0x00003: Version Magic (0x01000000)
 * 0x00004 - 0x00007: BIOS checksum (e.g. 0xBAAE187F for official BIOS)
 * 0x00008 - 0x0000F: Reserved (leave zero)
 * 0x00010 - 0x0001B: Game title (e.g. METROID4USA)
 * 0x0001C - 0x0001F: Game code (e.g. AMTE)
 * 0x00020 - 0x0012F: CPU state:
 * | 0x00020 - 0x0005F: GPRs
 * | 0x00060 - 0x00063: CPSR
 * | 0x00064 - 0x00067: SPSR
 * | 0x00068 - 0x0006B: Cycles since last event
 * | 0x0006C - 0x0006F: Cycles until next event
 * | 0x00070 - 0x00117: Banked registers
 * | 0x00118 - 0x0012F: Banked SPSRs
 * 0x00130 - 0x00147: Audio channel 1 state
 * | 0x00130 - 0x00130: Current volume
 * | 0x00131 - 0x00131: Is channel dead?
 * | 0x00132 - 0x00132: Is channel high?
 * | 0x00133 - 0x00133: Reserved
 * | 0x00134 - 0x00137: Next envelope step
 * | 0x00137 - 0x0013B: Next square wave step
 * | 0x0013C - 0x0013G: Next sweep step
 * | 0x00140 - 0x00143: Channel end cycle
 * | 0x00144 - 0x00147: Next event
 * 0x00148 - 0x0015F: Audio channel 2/4 state
 * | 0x00148 - 0x00148: Current volume
 * | 0x00149 - 0x00149: Is channel dead?
 * | 0x0014A - 0x0014A: Is channel high?
 * | 0x0014B - 0x0014B: Reserved
 * | 0x0014C - 0x0014F: Next envelope step
 * | 0x00150 - 0x00153: Next square wave step
 * | 0x00154 - 0x00157: Audio channel 4 LFSR
 * | 0x00158 - 0x0015B: Channel end cycle
 * | 0x0015C - 0x0015F: Next Event
 * 0x00160 - 0x0017F: Audio channel 3 wave banks
 * 0x00180 - 0x0019F: Audio FIFO 1
 * 0x001A0 - 0x001BF: Audio FIFO 2
 * 0x001C0 - 0x001DF: Audio miscellaneous state
 * | 0x001C0 - 0x001C3: Next event
 * | 0x001C4 - 0x001C7: Event diff
 * | 0x001C8 - 0x001CB: Next channel 3 event
 * | 0x001CC - 0x001CF: Next channel 4 event
 * | 0x001D0 - 0x001D3: Next sample
 * | 0x001D4 - 0x001D7: FIFO size
 * | 0x001D8 - 0x001DF: Reserved
 * 0x001E0 - 0x001FF: Video miscellaneous state
 * | 0x001E0 - 0x001E3: Next event
 * | 0x001E4 - 0x001E7: Event diff
 * | 0x001E8 - 0x001EB: Last hblank
 * | 0x001EC - 0x001EF: Next hblank
 * | 0x001F0 - 0x001F3: Next hblank IRQ
 * | 0x001F4 - 0x001F7: Next vblank IRQ
 * | 0x001F8 - 0x001FB: Next vcounter IRQ
 * | 0x001FC - 0x001FF: Reserved
 * 0x00200 - 0x003FF: Reserved (leave zero)
 * 0x00400 - 0x007FF: I/O memory
 * 0x00800 - 0x00BFF: Palette
 * 0x00C00 - 0x00FFF: OAM
 * 0x01000 - 0x18FFF: VRAM
 * 0x19000 - 0x20FFF: IWRAM
 * 0x21000 - 0x60FFF: WRAM
 * Total size: 0x61000 (397,312) bytes
 */

struct GBASerializedState {
	uint32_t versionMagic;
	uint32_t biosChecksum;
	uint32_t reservedHeader[2];

	char title[12];
	uint32_t id;

	struct {
		int32_t gprs[16];
		union PSR cpsr;
		union PSR spsr;

		int32_t cycles;
		int32_t nextEvent;

		int32_t bankedRegisters[6][7];
		int32_t bankedSPSRs[6];
	} cpu;

	struct {
		struct {
			int8_t volume;
			int8_t dead;
			int8_t hi;
			int8_t : 8;
			int32_t envelopeNextStep;
			int32_t waveNextStep;
			int32_t sweepNextStep;
			int32_t endTime;
			int32_t nextEvent;
		} ch1;
		struct {
			int8_t volume;
			int8_t dead;
			int8_t hi;
			int8_t : 8;
			int32_t envelopeNextStep;
			int32_t waveNextStep;
			int32_t ch4Lfsr;
			int32_t endTime;
			int32_t nextEvent;
		} ch2;
		uint32_t ch3[8];
		uint32_t fifoA[8];
		uint32_t fifoB[8];
		int32_t nextEvent;
		int32_t eventDiff;
		int32_t nextCh3;
		int32_t nextCh4;
		int32_t nextSample;
		int32_t fifoSize;
		int32_t : 32;
		int32_t : 32;
	} audio;

	struct {
		int32_t nextEvent;
		int32_t eventDiff;
		int32_t lastHblank;
		int32_t nextHblank;
		int32_t nextHblankIRQ;
		int32_t nextVblankIRQ;
		int32_t nextVcounterIRQ;
		int32_t : 32;
	} video;

	uint32_t reservedGpio[128];

	uint16_t io[SIZE_IO >> 1];
	uint16_t pram[SIZE_PALETTE_RAM >> 1];
	uint16_t oam[SIZE_OAM >> 1];
	uint16_t vram[SIZE_VRAM >> 1];
	uint8_t iwram[SIZE_WORKING_IRAM];
	uint8_t wram[SIZE_WORKING_RAM];
};

void GBASerialize(struct GBA* gba, struct GBASerializedState* state);
void GBADeserialize(struct GBA* gba, struct GBASerializedState* state);

int GBASaveState(struct GBA* gba, int slot);
int GBALoadState(struct GBA* gba, int slot);

struct GBASerializedState* GBAMapState(int fd);
struct GBASerializedState* GBAAllocateState(void);
void GBADeallocateState(struct GBASerializedState* state);

#endif
