/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "IOViewer.h"

#include "CoreController.h"
#include "GBAApp.h"

#include <QComboBox>
#include <QGridLayout>
#include <QSpinBox>

#ifdef M_CORE_GBA
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/memory.h>
#endif

#ifdef M_CORE_GB
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/memory.h>
#endif

struct ARMCore;

using namespace QGBA;

QHash<mPlatform, QList<IOViewer::RegisterDescription>> IOViewer::s_registers;

const QList<IOViewer::RegisterDescription>& IOViewer::registerDescriptions(mPlatform platform) {
	if (!s_registers.isEmpty()) {
		return s_registers[platform];
	}
#ifdef M_CORE_GBA
	QList<IOViewer::RegisterDescription> regGBA;
	// 0x04000000: DISPCNT
	regGBA.append({
		{ tr("Background mode"), 0, 3, {
			tr("Mode 0: 4 tile layers"),
			tr("Mode 1: 2 tile layers + 1 rotated/scaled tile layer"),
			tr("Mode 2: 2 rotated/scaled tile layers"),
			tr("Mode 3: Full 15-bit bitmap"),
			tr("Mode 4: Full 8-bit bitmap"),
			tr("Mode 5: Small 15-bit bitmap"),
			QString(),
			QString()
		} },
		{ tr("CGB Mode"), 3, 1, true },
		{ tr("Frame select"), 4 },
		{ tr("Unlocked HBlank"), 5 },
		{ tr("Linear OBJ tile mapping"), 6 },
		{ tr("Force blank screen"), 7 },
		{ tr("Enable background 0"), 8 },
		{ tr("Enable background 1"), 9 },
		{ tr("Enable background 2"), 10 },
		{ tr("Enable background 3"), 11 },
		{ tr("Enable OBJ"), 12 },
		{ tr("Enable Window 0"), 13 },
		{ tr("Enable Window 1"), 14 },
		{ tr("Enable OBJ Window"), 15 },
	});
	// 0x04000002: Green swap
	regGBA.append({
		{ tr("Swap green components"), 0 },
	});
	// 0x04000004: DISPSTAT
	regGBA.append({
		{ tr("Currently in VBlank"), 0, 1, true },
		{ tr("Currently in HBlank"), 1, 1, true },
		{ tr("Currently in VCounter"), 2, 1, true },
		{ tr("Enable VBlank IRQ generation"), 3 },
		{ tr("Enable HBlank IRQ generation"), 4 },
		{ tr("Enable VCounter IRQ generation"), 5 },
		{ tr("VCounter scanline"), 8, 8 },
	});
	// 0x04000006: VCOUNT
	regGBA.append({
		{ tr("Current scanline"), 0, 8, true },
	});
	// 0x04000008: BG0CNT
	regGBA.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x0400000A: BG1CNT
	regGBA.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x0400000C: BG2CNT
	regGBA.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Overflow wraps"), 13 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x0400000E: BG3CNT
	regGBA.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Overflow wraps"), 13 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x04000010: BG0HOFS
	regGBA.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x04000012: BG0VOFS
	regGBA.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x04000014: BG1HOFS
	regGBA.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x04000016: BG1VOFS
	regGBA.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x04000018: BG2HOFS
	regGBA.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x0400001A: BG2VOFS
	regGBA.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x0400001C: BG3HOFS
	regGBA.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x0400001E: BG3VOFS
	regGBA.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x04000020: BG2PA
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000022: BG2PB
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000024: BG2PC
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000026: BG2PD
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000028: BG2X_LO
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400002A: BG2X_HI
	regGBA.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x0400002C: BG2Y_LO
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400002E: BG2Y_HI
	regGBA.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x04000030: BG3PA
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000032: BG3PB
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000034: BG3PC
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000036: BG3PD
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000038: BG3X_LO
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400003A: BG3X_HI
	regGBA.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x0400003C: BG3Y_LO
	regGBA.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400003E: BG3Y_HI
	regGBA.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x04000040: WIN0H
	regGBA.append({
		{ tr("End x"), 0, 8 },
		{ tr("Start x"), 8, 8 },
	});
	// 0x04000042: WIN1H
	regGBA.append({
		{ tr("End x"), 0, 8 },
		{ tr("Start x"), 8, 8 },
	});
	// 0x04000044: WIN0V
	regGBA.append({
		{ tr("End y"), 0, 8 },
		{ tr("Start y"), 8, 8 },
	});
	// 0x04000046: WIN1V
	regGBA.append({
		{ tr("End y"), 0, 8 },
		{ tr("Start y"), 8, 8 },
	});
	// 0x04000048: WININ
	regGBA.append({
		{ tr("Window 0 enable BG 0"), 0 },
		{ tr("Window 0 enable BG 1"), 1 },
		{ tr("Window 0 enable BG 2"), 2 },
		{ tr("Window 0 enable BG 3"), 3 },
		{ tr("Window 0 enable OBJ"), 4 },
		{ tr("Window 0 enable blend"), 5 },
		{ tr("Window 1 enable BG 0"), 8 },
		{ tr("Window 1 enable BG 1"), 9 },
		{ tr("Window 1 enable BG 2"), 10 },
		{ tr("Window 1 enable BG 3"), 11 },
		{ tr("Window 1 enable OBJ"), 12 },
		{ tr("Window 1 enable blend"), 13 },
	});
	// 0x0400004A: WINOUT
	regGBA.append({
		{ tr("Outside window enable BG 0"), 0 },
		{ tr("Outside window enable BG 1"), 1 },
		{ tr("Outside window enable BG 2"), 2 },
		{ tr("Outside window enable BG 3"), 3 },
		{ tr("Outside window enable OBJ"), 4 },
		{ tr("Outside window enable blend"), 5 },
		{ tr("OBJ window enable BG 0"), 8 },
		{ tr("OBJ window enable BG 1"), 9 },
		{ tr("OBJ window enable BG 2"), 10 },
		{ tr("OBJ window enable BG 3"), 11 },
		{ tr("OBJ window enable OBJ"), 12 },
		{ tr("OBJ window enable blend"), 13 },
	});
	// 0x0400004C: MOSAIC
	regGBA.append({
		{ tr("Background mosaic size vertical"), 0, 4 },
		{ tr("Background mosaic size horizontal"), 4, 4 },
		{ tr("Object mosaic size vertical"), 8, 4 },
		{ tr("Object mosaic size horizontal"), 12, 4 },
	});
	// 0x0400004E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000050: BLDCNT
	regGBA.append({
		{ tr("BG 0 target 1"), 0 },
		{ tr("BG 1 target 1"), 1 },
		{ tr("BG 2 target 1"), 2 },
		{ tr("BG 3 target 1"), 3 },
		{ tr("OBJ target 1"), 4 },
		{ tr("Backdrop target 1"), 5 },
		{ tr("Blend mode"), 6, 2, {
			tr("Disabled"),
			tr("Additive blending"),
			tr("Brighten"),
			tr("Darken"),
		} },
		{ tr("BG 0 target 2"), 8 },
		{ tr("BG 1 target 2"), 9 },
		{ tr("BG 2 target 2"), 10 },
		{ tr("BG 3 target 2"), 11 },
		{ tr("OBJ target 2"), 12 },
		{ tr("Backdrop target 2"), 13 },
	});
	// 0x04000052: BLDALPHA
	regGBA.append({
		{ tr("Blend A (target 1)"), 0, 5 },
		{ tr("Blend B (target 2)"), 8, 5 },
	});
	// 0x04000054: BLDY
	regGBA.append({
		{ tr("Blend Y"), 0, 5 },
	});
	// 0x04000056: Unused
	regGBA.append(RegisterDescription());
	// 0x04000058: Unused
	regGBA.append(RegisterDescription());
	// 0x0400005A: Unused
	regGBA.append(RegisterDescription());
	// 0x0400005C: Unused
	regGBA.append(RegisterDescription());
	// 0x0400005E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000060: SOUND1CNT_LO
	regGBA.append({
		{ tr("Sweep shifts"), 0, 3 },
		{ tr("Sweep subtract"), 3 },
		{ tr("Sweep time (in 1/128s)"), 4, 3 },
	});
	// 0x04000062: SOUND1CNT_HI
	regGBA.append({
		{ tr("Sound length"), 0, 6 },
		{ tr("Duty cycle"),  6, 2 },
		{ tr("Envelope step time"), 8, 3 },
		{ tr("Envelope increase"), 11 },
		{ tr("Initial volume"), 12, 4 },
	});
	// 0x04000064: SOUND1CNT_X
	regGBA.append({
		{ tr("Sound frequency"), 0, 11 },
		{ tr("Timed"),  14 },
		{ tr("Reset"), 15 },
	});
	// 0x04000066: Unused
	regGBA.append(RegisterDescription());
	// 0x04000068: SOUND2CNT_LO
	regGBA.append({
		{ tr("Sound length"), 0, 6 },
		{ tr("Duty cycle"),  6, 2 },
		{ tr("Envelope step time"), 8, 3 },
		{ tr("Envelope increase"), 11 },
		{ tr("Initial volume"), 12, 4 },
	});
	// 0x0400006A: Unused
	regGBA.append(RegisterDescription());
	// 0x0400006C: SOUND2CNT_HI
	regGBA.append({
		{ tr("Sound frequency"), 0, 11 },
		{ tr("Timed"),  14 },
		{ tr("Reset"), 15 },
	});
	// 0x0400006E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000070: SOUND3CNT_LO
	regGBA.append({
		{ tr("Double-size wave table"), 5 },
		{ tr("Active wave table"),  6 },
		{ tr("Enable channel 3"), 7 },
	});
	// 0x04000072: SOUND3CNT_HI
	regGBA.append({
		{ tr("Sound length"), 0, 8 },
		{ tr("Volume"),  13, 3, {
			tr("0%"),
			tr("100%"),
			tr("50%"),
			tr("25%"),
			tr("75%"),
			tr("75%"),
			tr("75%"),
			tr("75%")
		} },
	});
	// 0x04000074: SOUND3CNT_X
	regGBA.append({
		{ tr("Sound frequency"), 0, 11 },
		{ tr("Timed"),  14 },
		{ tr("Reset"), 15 },
	});
	// 0x04000076: Unused
	regGBA.append(RegisterDescription());
	// 0x04000078: SOUND4CNT_LO
	regGBA.append({
		{ tr("Sound length"), 0, 6 },
		{ tr("Envelope step time"), 8, 3 },
		{ tr("Envelope increase"), 11 },
		{ tr("Initial volume"), 12, 4 },
	});
	// 0x0400007A: Unused
	regGBA.append(RegisterDescription());
	// 0x0400007C: SOUND4CNT_HI
	regGBA.append({
		{ tr("Clock divider"), 0, 3 },
		{ tr("Register stages"), 3, 1, {
			tr("15"),
			tr("7"),
		} },
		{ tr("Shifter frequency"), 4, 4 },
		{ tr("Timed"),  14 },
		{ tr("Reset"), 15 },
	});
	// 0x0400007E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000080: SOUNDCNT_LO
	regGBA.append({
		{ tr("PSG volume right"), 0, 3 },
		{ tr("PSG volume left"), 4, 3 },
		{ tr("Enable channel 1 right"), 8 },
		{ tr("Enable channel 2 right"), 9 },
		{ tr("Enable channel 3 right"), 10 },
		{ tr("Enable channel 4 right"), 11 },
		{ tr("Enable channel 1 left"), 12 },
		{ tr("Enable channel 2 left"), 13 },
		{ tr("Enable channel 3 left"), 14 },
		{ tr("Enable channel 4 left"), 15 },
	});
	// 0x04000082: SOUNDCNT_HI
	regGBA.append({
		{ tr("PSG master volume"), 0, 2, {
			tr("25%"),
			tr("50%"),
			tr("100%"),
			QString()
		} },
		{ tr("Loud channel A"), 2 },
		{ tr("Loud channel B"), 3 },
		{ tr("Enable channel A right"), 8 },
		{ tr("Enable channel A left"), 9 },
		{ tr("Channel A timer"), 10, 1, {
			tr("0"),
			tr("1"),
		} },
		{ tr("Channel A reset"), 11 },
		{ tr("Enable channel B right"), 12 },
		{ tr("Enable channel B left"), 13 },
		{ tr("Channel B timer"), 14, 1, {
			tr("0"),
			tr("1"),
		} },
		{ tr("Channel B reset"), 15 },
	});
	// 0x04000084: SOUNDCNT_LO
	regGBA.append({
		{ tr("Active channel 1"), 0, 1, true },
		{ tr("Active channel 2"), 1, 1, true },
		{ tr("Active channel 3"), 2, 1, true },
		{ tr("Active channel 4"), 3, 1, true },
		{ tr("Enable audio"), 7 },
	});
	// 0x04000086: Unused
	regGBA.append(RegisterDescription());
	// 0x04000088: SOUNDBIAS
	regGBA.append({
		{ tr("Bias"), 0, 10 },
		{ tr("Resolution"), 14, 2 },
	});
	// 0x0400008A: Unused
	regGBA.append(RegisterDescription());
	// 0x0400008C: Unused
	regGBA.append(RegisterDescription());
	// 0x0400008E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000090: WAVE_RAM0_LO
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000092: WAVE_RAM0_HI
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000094: WAVE_RAM1_LO
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000096: WAVE_RAM1_HI
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000098: WAVE_RAM2_LO
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x0400009A: WAVE_RAM2_HI
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x0400009C: WAVE_RAM3_LO
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x0400009E: WAVE_RAM0_HI
	regGBA.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x040000A0: FIFO_A_LO
	regGBA.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A2: FIFO_A_HI
	regGBA.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A4: FIFO_B_LO
	regGBA.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A6: FIFO_B_HI
	regGBA.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A8: Unused
	regGBA.append(RegisterDescription());
	// 0x040000AA: Unused
	regGBA.append(RegisterDescription());
	// 0x040000AC: Unused
	regGBA.append(RegisterDescription());
	// 0x040000AE: Unused
	regGBA.append(RegisterDescription());
	// 0x040000B0: DMA0SAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000B2: DMA0SAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000B4: DMA0DAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000B6: DMA0DAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000B8: DMA0CNT_LO
	regGBA.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000BA: DMA0CNT_HI
	regGBA.append({
		{ tr("Destination offset"), 5, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			tr("Increment and reload"),
		} },
		{ tr("Source offset"), 7, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			QString(),
		} },
		{ tr("Repeat"), 9 },
		{ tr("32-bit"), 10 },
		{ tr("Start timing"), 12, 2, {
			tr("Immediate"),
			tr("VBlank"),
			tr("HBlank"),
			QString(),
		} },
		{ tr("IRQ"), 14 },
		{ tr("Enable"), 15 },
	});
	// 0x040000BC: DMA1SAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000BE: DMA1SAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000C0: DMA1DAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000C2: DMA1DAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000C4: DMA1CNT_LO
	regGBA.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000C6: DMA1CNT_HI
	regGBA.append({
		{ tr("Destination offset"), 5, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			tr("Increment and reload"),
		} },
		{ tr("Source offset"), 7, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			QString(),
		} },
		{ tr("Repeat"), 9 },
		{ tr("32-bit"), 10 },
		{ tr("Start timing"), 12, 2, {
			tr("Immediate"),
			tr("VBlank"),
			tr("HBlank"),
			tr("Audio FIFO"),
		} },
		{ tr("IRQ"), 14 },
		{ tr("Enable"), 15 },
	});
	// 0x040000C8: DMA2SAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000CA: DMA2SAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000CC: DMA2DAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000CE: DMA2DAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000D0: DMA2CNT_LO
	regGBA.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000D2: DMA2CNT_HI
	regGBA.append({
		{ tr("Destination offset"), 5, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			tr("Increment and reload"),
		} },
		{ tr("Source offset"), 7, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			QString(),
		} },
		{ tr("Repeat"), 9 },
		{ tr("32-bit"), 10 },
		{ tr("Start timing"), 12, 2, {
			tr("Immediate"),
			tr("VBlank"),
			tr("HBlank"),
			tr("Audio FIFO"),
		} },
		{ tr("IRQ"), 14 },
		{ tr("Enable"), 15 },
	});
	// 0x040000D4: DMA3SAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000D6: DMA3SAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000D8: DMA3DAD_LO
	regGBA.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000DA: DMA3DAD_HI
	regGBA.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000DC: DMA3CNT_LO
	regGBA.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000DE: DMA3CNT_HI
	regGBA.append({
		{ tr("Destination offset"), 5, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			tr("Increment and reload"),
		} },
		{ tr("Source offset"), 7, 2, {
			tr("Increment"),
			tr("Decrement"),
			tr("Fixed"),
			tr("Video Capture"),
		} },
		{ tr("DRQ"), 8 },
		{ tr("Repeat"), 9 },
		{ tr("32-bit"), 10 },
		{ tr("Start timing"), 12, 2, {
			tr("Immediate"),
			tr("VBlank"),
			tr("HBlank"),
			tr("Audio FIFO"),
		} },
		{ tr("IRQ"), 14 },
		{ tr("Enable"), 15 },
	});
	// 0x040000E0: Unused
	regGBA.append(RegisterDescription());
	// 0x040000E2: Unused
	regGBA.append(RegisterDescription());
	// 0x040000E4: Unused
	regGBA.append(RegisterDescription());
	// 0x040000E6: Unused
	regGBA.append(RegisterDescription());
	// 0x040000E8: Unused
	regGBA.append(RegisterDescription());
	// 0x040000EA: Unused
	regGBA.append(RegisterDescription());
	// 0x040000EC: Unused
	regGBA.append(RegisterDescription());
	// 0x040000EE: Unused
	regGBA.append(RegisterDescription());
	// 0x040000F0: Unused
	regGBA.append(RegisterDescription());
	// 0x040000F2: Unused
	regGBA.append(RegisterDescription());
	// 0x040000F4: Unused
	regGBA.append(RegisterDescription());
	// 0x040000F6: Unused
	regGBA.append(RegisterDescription());
	// 0x040000F8: Unused
	regGBA.append(RegisterDescription());
	// 0x040000FA: Unused
	regGBA.append(RegisterDescription());
	// 0x040000FC: Unused
	regGBA.append(RegisterDescription());
	// 0x040000FE: Unused
	regGBA.append(RegisterDescription());
	// 0x04000100: TM0CNT_LO
	regGBA.append({
		{ tr("Value"), 0, 16 },
	});
	// 0x04000102: TM0CNT_HI
	regGBA.append({
		{ tr("Scale"), 0, 2, {
			tr("1"),
			tr("1/64"),
			tr("1/256"),
			tr("1/1024"),
		} },
		{ tr("IRQ"), 6 },
		{ tr("Enable"), 7 },
	});
	// 0x04000104: TM1CNT_LO
	regGBA.append({
		{ tr("Value"), 0, 16 },
	});
	// 0x04000106: TM1CNT_HI
	regGBA.append({
		{ tr("Scale"), 0, 2, {
			tr("1"),
			tr("1/64"),
			tr("1/256"),
			tr("1/1024"),
		} },
		{ tr("Cascade"), 2 },
		{ tr("IRQ"), 6 },
		{ tr("Enable"), 7 },
	});
	// 0x04000108: TM2CNT_LO
	regGBA.append({
		{ tr("Value"), 0, 16 },
	});
	// 0x0400010A: TM2CNT_HI
	regGBA.append({
		{ tr("Scale"), 0, 2, {
			tr("1"),
			tr("1/64"),
			tr("1/256"),
			tr("1/1024"),
		} },
		{ tr("Cascade"), 2 },
		{ tr("IRQ"), 6 },
		{ tr("Enable"), 7 },
	});
	// 0x0400010C: TM3CNT_LO
	regGBA.append({
		{ tr("Value"), 0, 16 },
	});
	// 0x0400010E: TM3CNT_HI
	regGBA.append({
		{ tr("Scale"), 0, 2, {
			tr("1"),
			tr("1/64"),
			tr("1/256"),
			tr("1/1024"),
		} },
		{ tr("Cascade"), 2 },
		{ tr("IRQ"), 6 },
		{ tr("Enable"), 7 },
	});
	// 0x04000110: Unused
	regGBA.append(RegisterDescription());
	// 0x04000112: Unused
	regGBA.append(RegisterDescription());
	// 0x04000114: Unused
	regGBA.append(RegisterDescription());
	// 0x04000116: Unused
	regGBA.append(RegisterDescription());
	// 0x04000118: Unused
	regGBA.append(RegisterDescription());
	// 0x0400011A: Unused
	regGBA.append(RegisterDescription());
	// 0x0400011C: Unused
	regGBA.append(RegisterDescription());
	// 0x0400011E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000120: SIOMULTI0
	regGBA.append(RegisterDescription());
	// 0x04000122: SIOMULTI1
	regGBA.append(RegisterDescription());
	// 0x04000124: SIOMULTI2
	regGBA.append(RegisterDescription());
	// 0x04000126: SIOMULTI3
	regGBA.append(RegisterDescription());
	// 0x04000128: SIOCNT
	regGBA.append(RegisterDescription());
	// 0x0400012A: SIOMLT_SEND
	regGBA.append(RegisterDescription());
	// 0x0400012C: Unused
	regGBA.append(RegisterDescription());
	// 0x0400012E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000130: KEYINPUT
	regGBA.append({
		{ tr("A"), 0 },
		{ tr("B"), 1 },
		{ tr("Select"), 2 },
		{ tr("Start"), 3 },
		{ tr("Right"), 4 },
		{ tr("Left"), 5 },
		{ tr("Up"), 6 },
		{ tr("Down"), 7 },
		{ tr("R"), 8 },
		{ tr("L"), 9 },
	});
	// 0x04000132: KEYCNT
	regGBA.append({
		{ tr("A"), 0 },
		{ tr("B"), 1 },
		{ tr("Select"), 2 },
		{ tr("Start"), 3 },
		{ tr("Right"), 4 },
		{ tr("Left"), 5 },
		{ tr("Up"), 6 },
		{ tr("Down"), 7 },
		{ tr("R"), 8 },
		{ tr("L"), 9 },
		{ tr("IRQ"), 14 },
		{ tr("Condition"), 15 },
	});
	// 0x04000134: RCNT
	regGBA.append({
		{ tr("SC"), 0 },
		{ tr("SD"), 1 },
		{ tr("SI"), 2 },
		{ tr("SO"), 3 },
	});
	// 0x04000136: Unused
	regGBA.append(RegisterDescription());
	// 0x04000138: SIOCNT
	regGBA.append(RegisterDescription());
	// 0x0400013A: Unused
	regGBA.append(RegisterDescription());
	// 0x0400013C: Unused
	regGBA.append(RegisterDescription());
	// 0x0400013E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000140: JOYCNT
	regGBA.append(RegisterDescription());
	// 0x04000142: Unused
	regGBA.append(RegisterDescription());
	// 0x04000144: Unused
	regGBA.append(RegisterDescription());
	// 0x04000146: Unused
	regGBA.append(RegisterDescription());
	// 0x04000148: Unused
	regGBA.append(RegisterDescription());
	// 0x0400014A: Unused
	regGBA.append(RegisterDescription());
	// 0x0400014C: Unused
	regGBA.append(RegisterDescription());
	// 0x0400014E: Unused
	regGBA.append(RegisterDescription());
	// 0x04000150: JOY_RECV_LO
	regGBA.append(RegisterDescription());
	// 0x04000152: JOY_RECV_HI
	regGBA.append(RegisterDescription());
	// 0x04000154: JOY_TRANS_LO
	regGBA.append(RegisterDescription());
	// 0x04000156: JOY_TRANS_HI
	regGBA.append(RegisterDescription());
	// 0x04000158: JOYSTAT
	regGBA.append(RegisterDescription());
	for (int i = 0x15A; i < 0x200; i += 2) {
		// Unused
		regGBA.append(RegisterDescription());
	}
	// 0x04000200: IE
	regGBA.append({
		{ tr("VBlank"), 0 },
		{ tr("HBlank"), 1 },
		{ tr("VCounter"), 2 },
		{ tr("Timer 0"), 3 },
		{ tr("Timer 1"), 4 },
		{ tr("Timer 2"), 5 },
		{ tr("Timer 3"), 6 },
		{ tr("SIO"), 7 },
		{ tr("DMA 0"), 8 },
		{ tr("DMA 1"), 9 },
		{ tr("DMA 2"), 10 },
		{ tr("DMA 3"), 11 },
		{ tr("Keypad"), 12 },
		{ tr("Gamepak"), 13 },
	});
	// 0x04000202: IF
	regGBA.append({
		{ tr("VBlank"), 0 },
		{ tr("HBlank"), 1 },
		{ tr("VCounter"), 2 },
		{ tr("Timer 0"), 3 },
		{ tr("Timer 1"), 4 },
		{ tr("Timer 2"), 5 },
		{ tr("Timer 3"), 6 },
		{ tr("SIO"), 7 },
		{ tr("DMA 0"), 8 },
		{ tr("DMA 1"), 9 },
		{ tr("DMA 2"), 10 },
		{ tr("DMA 3"), 11 },
		{ tr("Keypad"), 12 },
		{ tr("Gamepak"), 13 },
	});
	// 0x04000204: WAITCNT
	regGBA.append({
		{ tr("SRAM wait"), 0, 2, {
			tr("4"),
			tr("3"),
			tr("2"),
			tr("8"),
		} },
		{ tr("Cart 0 non-sequential"), 2, 2, {
			tr("4"),
			tr("3"),
			tr("2"),
			tr("8"),
		} },
		{ tr("Cart 0 sequential"), 4, 1, {
			tr("2"),
			tr("1"),
		} },
		{ tr("Cart 1 non-sequential"), 5, 2, {
			tr("4"),
			tr("3"),
			tr("2"),
			tr("8"),
		} },
		{ tr("Cart 1 sequential"), 7, 1, {
			tr("4"),
			tr("1"),
		} },
		{ tr("Cart 2 non-sequential"), 8, 2, {
			tr("4"),
			tr("3"),
			tr("2"),
			tr("8"),
		} },
		{ tr("Cart 2 sequential"), 10, 1, {
			tr("8"),
			tr("1"),
		} },
		{ tr("PHI terminal"), 11, 2, {
			tr("Disable"),
			tr("4.19MHz"),
			tr("8.38MHz"),
			tr("16.78MHz"),
		} },
		{ tr("Gamepak prefetch"), 14 },
	});
	// 0x04000206: Unused
	regGBA.append(RegisterDescription());
	// 0x04000208: IME
	regGBA.append({
		{ tr("Enable IRQs"), 0 },
	});
	s_registers[PLATFORM_GBA] = regGBA;
#endif
	return s_registers[platform];
}

IOViewer::IOViewer(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
	, m_controller(controller)
{
	m_ui.setupUi(this);
	const char* const* regs;
	unsigned maxRegs;
	switch (controller->platform()) {
#ifdef M_CORE_GB
	case PLATFORM_GB:
		regs = GBIORegisterNames;
		maxRegs = GB_REG_MAX;
		m_base = GB_BASE_IO;
		m_width = 0;
		break;
#endif
#ifdef M_CORE_GBA
	case PLATFORM_GBA:
		regs = GBAIORegisterNames;
		maxRegs = REG_MAX >> 1;
		m_base = BASE_IO;
		m_width = 1;
		break;
#endif
	}

	for (unsigned i = 0; i < maxRegs; ++i) {
		const char* reg = regs[i];
		if (!reg) {
			continue;
		}
		m_ui.regSelect->addItem("0x" + QString("%1: %2").arg((i << m_width) + m_base, 4, 16, QChar('0')).toUpper().arg(reg), i << m_width);
	}

	const QFont font = GBAApp::monospaceFont();
	m_ui.regValue->setFont(font);

	connect(m_ui.buttonBox, &QDialogButtonBox::clicked, this, &IOViewer::buttonPressed);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &QWidget::close);
	connect(m_ui.regSelect, &QComboBox::currentTextChanged,
	        this, static_cast<void (IOViewer::*)()>(&IOViewer::selectRegister));

	m_b[0] = m_ui.b0;
	m_b[1] = m_ui.b1;
	m_b[2] = m_ui.b2;
	m_b[3] = m_ui.b3;
	m_b[4] = m_ui.b4;
	m_b[5] = m_ui.b5;
	m_b[6] = m_ui.b6;
	m_b[7] = m_ui.b7;
	m_b[8] = m_ui.b8;
	m_b[9] = m_ui.b9;
	m_b[10] = m_ui.bA;
	m_b[11] = m_ui.bB;
	m_b[12] = m_ui.bC;
	m_b[13] = m_ui.bD;
	m_b[14] = m_ui.bE;
	m_b[15] = m_ui.bF;

	QWidget* l[16] = {
		m_ui.l0,
		m_ui.l1,
		m_ui.l2,
		m_ui.l3,
		m_ui.l4,
		m_ui.l5,
		m_ui.l6,
		m_ui.l7,
		m_ui.l8,
		m_ui.l9,
		m_ui.lA,
		m_ui.lB,
		m_ui.lC,
		m_ui.lD,
		m_ui.lE,
		m_ui.lF
	};

	for (int i = 0; i < (8 << m_width); ++i) {
		connect(m_b[i], &QAbstractButton::toggled, this, &IOViewer::bitFlipped);
	}

	for (int i = (8 << m_width) ; i < 16; ++i) {
		m_b[i]->hide();
		l[i]->hide();
	}

	selectRegister(0);

	connect(controller.get(), &CoreController::stopping, this, &QWidget::close);
}

void IOViewer::updateRegister() {
	{
		CoreController::Interrupter interrupter(m_controller);
		mCore* core = m_controller->thread()->core;
		switch (m_width) {
		case 0:
			m_value = core->rawRead8(core, m_base + m_register, -1);
			break;
		case 1:
			m_value = core->rawRead16(core, m_base + m_register, -1);
			break;
		}
	}

	for (int i = 0; i < (8 << m_width); ++i) {
		QSignalBlocker blocker(m_b[i]);
		m_b[i]->setChecked(m_value & (1 << i));
	}
	m_ui.regValue->setText("0x" + QString("%1").arg(m_value, (2 << m_width), 16, QChar('0')).toUpper());
	emit valueChanged();
}

void IOViewer::bitFlipped() {
	m_value = 0;
	for (int i = 0; i < (8 << m_width); ++i) {
		m_value |= m_b[i]->isChecked() << i;
	}
	m_ui.regValue->setText("0x" + QString("%1").arg(m_value, (2 << m_width), 16, QChar('0')).toUpper());
	emit valueChanged();
}

void IOViewer::writeback() {
	{
		CoreController::Interrupter interrupter(m_controller);
		GBAIOWrite(static_cast<GBA*>(m_controller->thread()->core->board), m_register, m_value);
	}
	updateRegister();
}

void IOViewer::selectRegister(int address) {
	m_register = address;
	QGridLayout* box = static_cast<QGridLayout*>(m_ui.regDescription->layout());
	if (box) {
		// I can't believe there isn't a real way to do this...
		while (!box->isEmpty()) {
			QLayoutItem* item = box->takeAt(0);
			if (item->widget()) {
				delete item->widget();
			}
			delete item;
		}
	} else {
		box = new QGridLayout;
	}
	if (registerDescriptions(m_controller->platform()).count() <= address >> m_width) {
		return;
	}
	const RegisterDescription& description = registerDescriptions(m_controller->platform()).at(address >> m_width);
	int i = 0;
	for (const RegisterItem& ri : description) {
		QLabel* label = new QLabel(ri.description);
		box->addWidget(label, i, 0);
		if (ri.size == 1) {
			QCheckBox* check = new QCheckBox;
			check->setEnabled(!ri.readonly);
			box->addWidget(check, i, 1, Qt::AlignRight);
			connect(check, &QAbstractButton::toggled, m_b[ri.start], &QAbstractButton::setChecked);
			connect(this, &IOViewer::valueChanged, check, [check, this, &ri] {
				QSignalBlocker blocker(check);
				check->setChecked(bool(m_value & (1 << ri.start)));
			});
		} else if (ri.items.empty()) {
			QSpinBox* sbox = new QSpinBox;
			sbox->setEnabled(!ri.readonly);
			sbox->setMaximum((1 << ri.size) - 1);
			sbox->setAccelerated(true);
			box->addWidget(sbox, i, 1, Qt::AlignRight);

			connect(sbox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [sbox, this, &ri](int v) {
				for (int o = 0; o < ri.size; ++o) {
					QSignalBlocker blocker(m_b[o + ri.start]);
					m_b[o + ri.start]->setChecked(v & (1 << o));
				}
				bitFlipped();
			});

			connect(this, &IOViewer::valueChanged, sbox, [sbox, this, &ri]() {
				QSignalBlocker blocker(sbox);
				int v = (m_value >> ri.start) & ((1 << ri.size) - 1);
				sbox->setValue(v);
			});
		} else {
			QComboBox* cbox = new QComboBox;
			cbox->setEnabled(!ri.readonly);
			++i;
			box->addWidget(cbox, i, 0, 1, 2, Qt::AlignRight);
			for (int o = 0; o < 1 << ri.size; ++o) {
				if (ri.items.at(o).isNull()) {
					continue;
				}
				cbox->addItem(ri.items.at(o), o);
			}

			connect(cbox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [cbox, this, &ri](int index) {
				unsigned v = cbox->itemData(index).toUInt();
				for (int o = 0; o < ri.size; ++o) {
					QSignalBlocker blocker(m_b[o + ri.start]);
					m_b[o + ri.start]->setChecked(v & (1 << o));
				}
				bitFlipped();
			});

			connect(this, &IOViewer::valueChanged, cbox, [cbox, this, &ri]() {
				QSignalBlocker blocker(cbox);
				unsigned v = (m_value >> ri.start) & ((1 << ri.size) - 1);
				for (int i = 0; i < 1 << ri.size; ++i) {
					if (cbox->itemData(i) == v) {
						cbox->setCurrentIndex(i);
						break;
					}
				}
			});
		}
		++i;
	}
	m_ui.regDescription->setLayout(box);
	updateRegister();
}

void IOViewer::selectRegister() {
	selectRegister(m_ui.regSelect->currentData().toUInt());
}

void IOViewer::buttonPressed(QAbstractButton* button) {
	switch (m_ui.buttonBox->standardButton(button)) {
	case QDialogButtonBox::Reset:
		updateRegister();
	 	break;
	case QDialogButtonBox::Apply:
	 	writeback();
	 	break;
	default:
		break;
	}
}
