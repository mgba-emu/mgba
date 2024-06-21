/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SERIALIZE_H
#define GBA_SERIALIZE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/core.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gb/serialize.h>

extern MGBA_EXPORT const uint32_t GBASavestateMagic;
extern MGBA_EXPORT const uint32_t GBASavestateVersion;

mLOG_DECLARE_CATEGORY(GBA_STATE);

/* Savestate format:
 * 0x00000 - 0x00003: Version Magic (0x01000006)
 * 0x00004 - 0x00007: BIOS checksum (e.g. 0xBAAE187F for official BIOS)
 * 0x00008 - 0x0000B: ROM CRC32
 * 0x0000C - 0x0000F: Master cycles
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
 * 0x00130 - 0x00143: Audio channel 1/framer state
 * | 0x00130 - 0x00133: Envelepe timing
 *   | bits 0 - 6: Remaining length
 *   | bits 7 - 9: Next step
 *   | bits 10 - 20: Shadow frequency register
 *   | bits 21 - 23: Duty index
 *   | bits 24 - 31: Reserved
 * | 0x00134 - 0x00137: Next frame
 * | 0x00138 - 0x0013B: Reserved
 * | 0x0013C - 0x0013F: Sweep state
 *   | bits 0 - 2: Timesteps
 *   | bits 3 - 7: Reserved
 * | 0x00140 - 0x00143: Last update
 * 0x00144 - 0x00153: Audio channel 2 state
 * | 0x00144 - 0x00147: Envelepe timing
 *   | bits 0 - 2: Remaining length
 *   | bits 3 - 5: Next step
 *   | bits 6 - 20: Reserved
 *   | bits 21 - 23: Duty index
 *   | bits 24 - 31: Reserved
 * | 0x00148 - 0x0014F: Reserved
 * | 0x00150 - 0x00153: Last update
 * 0x00154 - 0x0017B: Audio channel 3 state
 * | 0x00154 - 0x00173: Wave banks
 * | 0x00174 - 0x00175: Remaining length
 * | 0x00176 - 0x00177: Reserved
 * | 0x00178 - 0x0017B: Next event
 * 0x0017C - 0x0018B: Audio channel 4 state
 * | 0x0017C - 0x0017F: Linear feedback shift register state
 * | 0x00180 - 0x00183: Envelepe timing
 *   | bits 0 - 2: Remaining length
 *   | bits 3 - 5: Next step
 *   | bits 6 - 31: Reserved
 * | 0x00184 - 0x00187: Last event
 * | 0x00188 - 0x0018B: Next event
 * 0x0018C - 0x001AB: Audio FIFO 1
 * 0x001AC - 0x001CB: Audio FIFO 2
 * 0x001CC - 0x001EF: Audio miscellaneous state
 * | 0x001CC - 0x001CF: Channel A internal audio samples
 * | 0x001D0 - 0x001D3: Channel B internal audio samples
 * | 0x001D4 - 0x001D7: Next sample
 * | 0x001D8: Channel A current sample
 * | 0x001D9: Channel B current sample
 * | 0x001DA - 0x001DB: Flags
 *   | bits 0 - 1: Channel B internal samples remaining
 *   | bits 2 - 4: Channel B readable words
 *   | bits 5 - 6: Channel A internal samples remaining
 *   | bits 7 - 9: Channel A readable words
 * | TODO: Fix this, they're in big-endian order, but field is little-endian
 * | 0x001DC - 0x001DC: Channel 1 envelope state
 *   | bits 0 - 3: Current volume
 *   | bits 4 - 5: Is dead?
 *   | bit 6: Is high?
*    | bit 7: Reserved
 * | 0x001DD - 0x001DD: Channel 2 envelope state
 *   | bits 0 - 3: Current volume
 *   | bits 4 - 5: Is dead?
 *   | bit 6: Is high?
*    | bit 7: Reserved
 * | 0x001DE - 0x001DE: Channel 4 envelope state
 *   | bits 0 - 3: Current volume
 *   | bits 4 - 5: Is dead?
 *   | bits 6 - 7: Current frame (continued)
 * | 0x001DF - 0x001DF: Miscellaneous audio flags
 *   | bit 0: Current frame (continuation)
 *   | bit 1: Is channel 1 sweep enabled?
 *   | bit 2: Has channel 1 sweep occurred?
 *   | bit 3: Is channel 3's memory readable?
 *   | bit 4: Skip frame
 *   | bits 5 - 7: Reserved
 * | 0x001E0 - 0x001E3: Last sample
 * | 0x001E4 - 0x001E7: Additional audio flags
 *   | bits 0 - 3: Current sample index
 * | 0x001E8 - 0x001EF: Reserved
 * 0x001F0 - 0x001FF: Video miscellaneous state
 * | 0x001F0 - 0x001F3: Reserved
 * | 0x001F4 - 0x001F7: Next event
 * | 0x001F8 - 0x001FB: Miscellaneous flags
 * | 0x001FC - 0x001FF: Frame counter
 * 0x00200 - 0x00213: Timer 0
 * | 0x00200 - 0x00201: Reload value
 * | 0x00202 - 0x00203: Old reload value
 * | 0x00204 - 0x00207: Last event
 * | 0x00208 - 0x0020B: Next event
 * | 0x0020C - 0x0020F: Reserved
 * | 0x00210 - 0x00213: Miscellaneous flags
 * 0x00214 - 0x00227: Timer 1
 * | 0x00214 - 0x00215: Reload value
 * | 0x00216 - 0x00217: Old reload value
 * | 0x00218 - 0x0021B: Last event
 * | 0x0021C - 0x0021F: Next event
 * | 0x00220 - 0x00223: Reserved
 * | 0x00224 - 0x00227: Miscellaneous flags
 * 0x00228 - 0x0023B: Timer 2
 * | 0x00228 - 0x00229: Reload value
 * | 0x0022A - 0x0022B: Old reload value
 * | 0x0022C - 0x0022F: Last event
 * | 0x00230 - 0x00233: Next event
 * | 0x00234 - 0x00237: Reserved
 * | 0x00238 - 0x0023B: Miscellaneous flags
 * 0x0023C - 0x00250: Timer 3
 * | 0x0023C - 0x0023D: Reload value
 * | 0x0023E - 0x0023F: Old reload value
 * | 0x00240 - 0x00243: Last event
 * | 0x00244 - 0x00247: Next event
 * | 0x00248 - 0x0024B: Reserved
 * | 0x0024C - 0x0024F: Miscellaneous flags
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
 * 0x00290 - 0x002C3: GPIO state
 * | 0x00290 - 0x00291: Pin state
 * | 0x00292 - 0x00293: Direction state
 * | 0x00294 - 0x002B6: RTC state (see hardware.h for format)
 * | 0x002B7 - 0x002B7: GPIO devices
 *   | bit 0: Has RTC values
 *   | bit 1: Has rumble value (reserved)
 *   | bit 2: Has light sensor value
 *   | bit 3: Has gyroscope value
 *   | bit 4: Has tilt values
 *   | bit 5: Has Game Boy Player attached
 *   | bits 6 - 7: Reserved
 * | 0x002B8 - 0x002B9: Gyroscope sample
 * | 0x002BA - 0x002BB: Tilt x sample
 * | 0x002BC - 0x002BD: Tilt y sample
 * | 0x002BE - 0x002BF: Flags
 *   | bit 0: Is read enabled
 *   | bit 1: Gyroscope sample is edge
 *   | bit 2: Light sample is edge
 *   | bit 3: Reserved
 *   | bits 4 - 15: Light counter
 * | 0x002C0 - 0x002C0: Light sample
 * | 0x002C1 - 0x002C3: Flags
 *   | bits 0 - 1: Tilt state machine
 *   | bits 2 - 3: GB Player inputs posted
 *   | bits 4 - 8: GB Player transmit position
 *   | bits 9 - 23: Reserved
 * 0x002C4 - 0x002C7: Game Boy Player next event
 * 0x002C8 - 0x002CB: Current DMA transfer word
 * 0x002CC - 0x002CF: Last DMA transfer PC
 * 0x002D0 - 0x002DF: Matrix memory command buffer
 * | 0x002D0 - 0x002D3: Command
 * | 0x002D4 - 0x002D7: Physical address
 * | 0x002D8 - 0x002DB: Virtual address
 * | 0x002DC - 0x002DF: Size
 * 0x002E0 - 0x002EF: Savedata state
 * | 0x002E0 - 0x002E0: Savedata type
 * | 0x002E1 - 0x002E1: Savedata command (see savedata.h)
 * | 0x002E2 - 0x002E2: Flags
 *   | bits 0 - 1: Flash state machine
 *   | bits 2 - 3: Reserved
 *   | bit 4: Flash bank
 *   | bit 5: Is settling occurring?
 *   | bits 6 - 7: Reserved
 * | 0x002E3 - 0x002E3: EEPROM read bits remaining
 * | 0x002E4 - 0x002E7: Settling cycles remaining
 * | 0x002E8 - 0x002EB: EEPROM read address
 * | 0x002EC - 0x002EF: EEPROM write address
 * | 0x002F0 - 0x002F1: Flash settling sector
 * | 0x002F2 - 0x002F3: Reserved
 * 0x002F4 - 0x002FF: Prefetch
 * | 0x002F4 - 0x002F7: GBA BIOS bus prefetch
 * | 0x002F8 - 0x002FB: CPU prefecth (decode slot)
 * | 0x002FC - 0x002FF: CPU prefetch (fetch slot)
 * 0x00300 - 0x0030F: Reserved (leave zero)
 * 0x00310 - 0x00317: Global cycle counter
 * 0x00318 - 0x0031B: Last prefetched program counter
 * 0x0031C - 0x0031F: Miscellaneous flags
 * | bit 0: Is CPU halted?
 * | bit 1: POSTFLG
 * | bit 2: Is IRQ pending?
 * | bit 3: Is CPU blocked?
 * | bits 4 - 14: Active key IRQ keys
 * | bits 15 - 31: Reserved
 * 0x00320 - 0x00323: Next IRQ event
 * 0x00324 - 0x00327: Interruptable BIOS stall cycles
 * 0x00328 - 0x00367: Matrix memory mapping table
 * 0x00368 - 0x0036F: Reserved (leave zero)
 * 0x00370 - 0x0037F: Audio FIFO A samples
 * 0x00380 - 0x0038F: Audio FIFO B samples
 * 0x00390 - 0x003CF: Audio rendered samples
 * 0x003D0 - 0x003FF: Reserved (leave zero)
 * 0x00400 - 0x007FF: I/O memory
 * 0x00800 - 0x00BFF: Palette
 * 0x00C00 - 0x00FFF: OAM
 * 0x01000 - 0x18FFF: VRAM
 * 0x19000 - 0x20FFF: IWRAM
 * 0x21000 - 0x60FFF: WRAM
 * Total size: 0x61000 (397,312) bytes
 */

DECL_BITFIELD(GBASerializedAudioFlags, uint16_t);
DECL_BITS(GBASerializedAudioFlags, FIFOInternalSamplesB, 0, 2);
DECL_BITS(GBASerializedAudioFlags, FIFOSamplesB, 2, 3); // Yay legacy?
DECL_BITS(GBASerializedAudioFlags, FIFOInternalSamplesA, 5, 2);
DECL_BITS(GBASerializedAudioFlags, FIFOSamplesA, 7, 3);

DECL_BITFIELD(GBASerializedAudioFlags2, uint32_t);
DECL_BITS(GBASerializedAudioFlags2, SampleIndex, 0, 4);

DECL_BITFIELD(GBASerializedVideoFlags, uint32_t);
DECL_BITS(GBASerializedVideoFlags, Mode, 0, 2);

DECL_BITFIELD(GBASerializedHWFlags1, uint16_t);
DECL_BIT(GBASerializedHWFlags1, ReadWrite, 0);
DECL_BIT(GBASerializedHWFlags1, GyroEdge, 1);
DECL_BIT(GBASerializedHWFlags1, LightEdge, 2);
DECL_BITS(GBASerializedHWFlags1, LightCounter, 4, 12);

DECL_BITFIELD(GBASerializedHWFlags2, uint8_t);
DECL_BITS(GBASerializedHWFlags2, TiltState, 0, 2);
DECL_BITS(GBASerializedHWFlags2, GbpInputsPosted, 2, 2);
DECL_BITS(GBASerializedHWFlags2, GbpTxPosition, 4, 5);

DECL_BITFIELD(GBASerializedHWFlags3, uint16_t);

DECL_BITFIELD(GBASerializedSavedataFlags, uint8_t);
DECL_BITS(GBASerializedSavedataFlags, FlashState, 0, 2);
DECL_BIT(GBASerializedSavedataFlags, FlashBank, 4);
DECL_BIT(GBASerializedSavedataFlags, DustSettling, 5);

DECL_BITFIELD(GBASerializedMiscFlags, uint32_t);
DECL_BIT(GBASerializedMiscFlags, Halted, 0);
DECL_BIT(GBASerializedMiscFlags, POSTFLG, 1);
DECL_BIT(GBASerializedMiscFlags, IrqPending, 2);
DECL_BIT(GBASerializedMiscFlags, Blocked, 3);
DECL_BITS(GBASerializedMiscFlags, KeyIRQKeys, 4, 11);

struct GBASerializedState {
	uint32_t versionMagic;
	uint32_t biosChecksum;
	uint32_t romCrc32;
	uint32_t masterCycles;

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
		struct GBSerializedPSGState psg;
		uint32_t fifoA[8];
		uint32_t fifoB[8];
		uint32_t internalA;
		uint32_t internalB;
		int32_t nextSample;
		int8_t sampleA;
		int8_t sampleB;
		GBASerializedAudioFlags gbaFlags;
		GBSerializedAudioFlags flags;
		int32_t lastSample;
		GBASerializedAudioFlags2 gbaFlags2;
		int32_t reserved[2];
	} audio;

	struct {
		int32_t reserved;
		int32_t nextEvent;
		GBASerializedVideoFlags flags;
		uint32_t frameCounter;
	} video;

	struct {
		uint16_t reload;
		uint16_t reserved0;
		uint32_t lastEvent;
		uint32_t nextEvent;
		uint32_t reserved1;
		GBATimerFlags flags;
	} timers[4];

	struct {
		uint32_t nextSource;
		uint32_t nextDest;
		int32_t nextCount;
		int32_t when;
	} dma[4];

	struct {
		uint16_t pinState;
		uint16_t pinDirection;
		int32_t rtcBytesRemaining;
		int32_t rtcTransferStep;
		int32_t rtcBitsRead;
		int32_t rtcBits;
		int32_t rtcCommandActive;
		RTCCommandData rtcCommand;
		uint8_t rtcControl;
		uint8_t reserved[3];
		uint8_t time[7];
		uint8_t devices;
		uint16_t gyroSample;
		uint16_t tiltSampleX;
		uint16_t tiltSampleY;
		GBASerializedHWFlags1 flags1;
		uint8_t lightSample;
		GBASerializedHWFlags2 flags2;
		GBASerializedHWFlags3 flags3;
		uint32_t gbpNextEvent;
	} hw;

	uint32_t dmaTransferRegister;
	uint32_t dmaBlockPC;

	struct {
		uint32_t cmd;
		uint32_t paddr;
		uint32_t vaddr;
		uint32_t size;
	} matrix;

	struct {
		uint8_t type;
		uint8_t command;
		GBASerializedSavedataFlags flags;
		int8_t readBitsRemaining;
		uint32_t settlingDust;
		uint32_t readAddress;
		uint32_t writeAddress;
		uint16_t settlingSector;
		uint16_t reserved;
	} savedata;

	uint32_t biosPrefetch;
	uint32_t cpuPrefetch[2];

	uint32_t reservedCpu[4];

	uint64_t globalCycles;
	uint32_t lastPrefetchedPc;
	GBASerializedMiscFlags miscFlags;
	uint32_t nextIrq;
	int32_t biosStall;

	uint32_t matrixMappings[16];
	uint32_t reservedMatrix[2];

	struct {
		int8_t chA[16];
		int8_t chB[16];
	} samples;

	struct mStereoSample currentSamples[GBA_MAX_SAMPLES];

	uint32_t reserved[12];

	uint16_t io[GBA_SIZE_IO >> 1];
	uint16_t pram[GBA_SIZE_PALETTE_RAM >> 1];
	uint16_t oam[GBA_SIZE_OAM >> 1];
	uint16_t vram[GBA_SIZE_VRAM >> 1];
	uint8_t iwram[GBA_SIZE_IWRAM];
	uint8_t wram[GBA_SIZE_EWRAM];
};

static_assert(sizeof(struct GBASerializedState) == 0x61000, "GBA savestate struct sized wrong");

struct VDir;

void GBASerialize(struct GBA* gba, struct GBASerializedState* state);
bool GBADeserialize(struct GBA* gba, const struct GBASerializedState* state);

CXX_GUARD_END

#endif
