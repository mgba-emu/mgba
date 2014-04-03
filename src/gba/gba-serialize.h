#ifndef GBA_SERIALIZE_H
#define GBA_SERIALIZE_H

#include "common.h"

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
 * 0x00130 - 0x00143: Audio channel 1 state
 * | 0x00130 - 0x00133: Next envelope step
 * | 0x00134 - 0x00137: Next square wave step
 * | 0x00138 - 0x0013B: Next sweep step
 * | 0x0013C - 0x0013F: Channel end cycle
 * | 0x00140 - 0x00143: Next event
 * 0x00144 - 0x00153: Audio channel 2 state
 * | 0x00144 - 0x00147: Next envelope step
 * | 0x00148 - 0x0014B: Next square wave step
 * | 0x0014C - 0x0014F: Channel end cycle
 * | 0x00150 - 0x00153: Next event
 * 0x00154 - 0x0017B: Audio channel 3 state
 * | 0x00154 - 0x00173: Wave banks
 * | 0x00174 - 0x00177: Channel end cycle
 * | 0x00178 - 0x0017B: Next event
 * 0x0017C - 0x0018B: Audio channel 4 state
 * | 0x0017C - 0x0017F: Linear feedback shift register state
 * | 0x00180 - 0x00183: Next enveleope step
 * | 0x00184 - 0x00187: Channel end cycle
 * | 0x00188 - 0x0018B: Next event
 * 0x0018C - 0x001AB: Audio FIFO 1
 * 0x001AC - 0x001CB: Audio FIFO 2
 * 0x001CC - 0x001DF: Audio miscellaneous state
 * | 0x001CC - 0x001CF: Next event
 * | 0x001D0 - 0x001D3: Event diff
 * | 0x001D4 - 0x001D7: Next sample
 * | 0x001D8 - 0x001DB: FIFO size
 * | 0x001DC - 0x001DC: Channel 1 envelope state
 *   | bits 0 - 3: Current volume
 *   | bit 4: Is dead?
 *   | bit 5: Is high?
 *   | bits 6 - 7: Reserved
 * | 0x001DD - 0x001DD: Channel 2 envelope state
 *   | bits 0 - 3: Current volume
 *   | bit 4: Is dead?
 *   | bit 5: Is high?
 *   | bits 6 - 7: Reserved
 * | 0x001DE - 0x001DE: Channel 4 envelope state
 *   | bits 0 - 3: Current volume
 *   | bit 4: Is dead?
 *   | bits 5 - 7: Reserved
 * | 0x001DF - 0x001DF: Reserved
 * 0x001E0 - 0x001FF: Video miscellaneous state
 * | 0x001E0 - 0x001E3: Next event
 * | 0x001E4 - 0x001E7: Event diff
 * | 0x001E8 - 0x001EB: Last hblank
 * | 0x001EC - 0x001EF: Next hblank
 * | 0x001F0 - 0x001F3: Next hblank IRQ
 * | 0x001F4 - 0x001F7: Next vblank IRQ
 * | 0x001F8 - 0x001FB: Next vcounter IRQ
 * | 0x001FC - 0x001FF: Reserved
 * 0x00200 - 0x00213: Timer 0
 * | 0x00200 - 0x00201: Reload value
 * | 0x00202 - 0x00203: Old reload value
 * | 0x00204 - 0x00207: Last event
 * | 0x00208 - 0x0020B: Next event
 * | 0x0020C - 0x0020F: Overflow interval
 * | 0x00210 - 0x00213: Miscellaenous flags
 * 0x00214 - 0x00227: Timer 1
 * | 0x00214 - 0x00215: Reload value
 * | 0x00216 - 0x00217: Old reload value
 * | 0x00218 - 0x0021B: Last event
 * | 0x0021C - 0x0021F: Next event
 * | 0x00220 - 0x00223: Overflow interval
 * | 0x00224 - 0x00227: Miscellaenous flags
 * 0x00228 - 0x0023B: Timer 2
 * | 0x00228 - 0x00229: Reload value
 * | 0x0022A - 0x0022B: Old reload value
 * | 0x0022C - 0x0022F: Last event
 * | 0x00230 - 0x00233: Next event
 * | 0x00234 - 0x00237: Overflow interval
 * | 0x00238 - 0x0023B: Miscellaenous flags
 * 0x0023C - 0x00250: Timer 3
 * | 0x0023C - 0x0023D: Reload value
 * | 0x0023E - 0x0023F: Old reload value
 * | 0x00240 - 0x00243: Last event
 * | 0x00244 - 0x00247: Next event
 * | 0x00248 - 0x0024B: Overflow interval
 * | 0x0024C - 0x0024F: Miscellaenous flags
 * 0x00250 - 0x0025F: DMA 0
 * | 0x00250 - 0x00253: DMA next source
 * | 0x00254 - 0x00257: DMA next destination
 * | 0x00258 - 0x0025B: DMA next count
 * | 0x0025C - 0x0025F: DMA next event
 * 0x00260 - 0x0026F: DMA 1
 * | 0x00260 - 0x00263: DMA next source
 * | 0x00264 - 0x00267: DMA next destination
 * | 0x00268 - 0x0026B: DMA next count
 * | 0x0026C - 0x0026F: DMA next event
 * 0x00270 - 0x0027F: DMA 2
 * | 0x00270 - 0x00273: DMA next source
 * | 0x00274 - 0x00277: DMA next destination
 * | 0x00278 - 0x0027B: DMA next count
 * | 0x0027C - 0x0027F: DMA next event
 * 0x00280 - 0x0028F: DMA 3
 * | 0x00280 - 0x00283: DMA next source
 * | 0x00284 - 0x00287: DMA next destination
 * | 0x00288 - 0x0028B: DMA next count
 * | 0x0028C - 0x0028F: DMA next event
 * 0x00290 - 0x003FF: Reserved (leave zero)
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
			int32_t envelopeNextStep;
			int32_t waveNextStep;
			int32_t sweepNextStep;
			int32_t endTime;
			int32_t nextEvent;
		} ch1;
		struct {
			int32_t envelopeNextStep;
			int32_t waveNextStep;
			int32_t endTime;
			int32_t nextEvent;
		} ch2;
		struct {
			uint32_t wavebanks[8];
			int32_t endTime;
			int32_t nextEvent;
		} ch3;
		struct {
			int32_t lfsr;
			int32_t envelopeNextStep;
			int32_t endTime;
			int32_t nextEvent;
		} ch4;
		uint32_t fifoA[8];
		uint32_t fifoB[8];
		int32_t nextEvent;
		int32_t eventDiff;
		int32_t nextSample;
		int32_t fifoSize;
		unsigned ch1Volume : 4;
		unsigned ch1Dead : 1;
		unsigned ch1Hi : 1;
		unsigned : 2;
		unsigned ch2Volume : 4;
		unsigned ch2Dead : 1;
		unsigned ch2Hi : 1;
		unsigned : 2;
		unsigned ch4Volume : 4;
		unsigned ch4Dead : 1;
		unsigned : 3;
		unsigned : 8;
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

	struct GBATimer timers[4];

	struct {
		uint32_t nextSource;
		uint32_t nextDest;
		int32_t nextCount;
		int32_t nextEvent;
	} dma[4];

	uint32_t reservedGpio[92];

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

struct GBAThread;
void GBARecordFrame(struct GBAThread* thread);
void GBARewind(struct GBAThread* thread, int nStates);

#endif
