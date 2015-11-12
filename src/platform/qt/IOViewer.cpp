/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "IOViewer.h"

#include "GameController.h"

#include <QFontDatabase>
#include <QGridLayout>
#include <QRadioButton>
#include <QSpinBox>

extern "C" {
#include "gba/io.h"
}

using namespace QGBA;


QList<IOViewer::RegisterDescription> IOViewer::s_registers;

const QList<IOViewer::RegisterDescription>& IOViewer::registerDescriptions() {
	if (!s_registers.isEmpty()) {
		return s_registers;
	}
	// 0x04000000: DISPCNT
	s_registers.append({
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
	// 0x04000002: Green swap (undocumented and unimplemented)
	s_registers.append(RegisterDescription());
	// 0x04000004: DISPSTAT
	s_registers.append({
		{ tr("Currently in VBlank"), 0, 1, true },
		{ tr("Currently in HBlank"), 1, 1, true },
		{ tr("Currently in VCounter"), 2, 1, true },
		{ tr("Enable VBlank IRQ generation"), 3 },
		{ tr("Enable HBlank IRQ generation"), 4 },
		{ tr("Enable VCounter IRQ generation"), 5 },
		{ tr("VCounter scanline"), 8, 8 },
	});
	// 0x04000006: VCOUNT
	s_registers.append({
		{ tr("Current scanline"), 0, 8, true },
	});
	// 0x04000008: BG0CNT
	s_registers.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x0400000A: BG1CNT
	s_registers.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x0400000C: BG2CNT
	s_registers.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Overflow wraps"), 13 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x0400000E: BG3CNT
	s_registers.append({
		{ tr("Priority"), 0, 2 },
		{ tr("Tile data base (* 16kB)"), 2, 2 },
		{ tr("Enable mosaic"), 6 },
		{ tr("Enable 256-color"), 7 },
		{ tr("Tile map base (* 2kB)"), 8, 5 },
		{ tr("Overflow wraps"), 13 },
		{ tr("Background dimensions"), 14, 2 },
	});
	// 0x04000010: BG0HOFS
	s_registers.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x04000012: BG0VOFS
	s_registers.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x04000014: BG1HOFS
	s_registers.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x04000016: BG1VOFS
	s_registers.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x04000018: BG2HOFS
	s_registers.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x0400001A: BG2VOFS
	s_registers.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x0400001C: BG3HOFS
	s_registers.append({
		{ tr("Horizontal offset"), 0, 9 },
	});
	// 0x0400001E: BG3VOFS
	s_registers.append({
		{ tr("Vertical offset"), 0, 9 },
	});
	// 0x04000020: BG2PA
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000022: BG2PB
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000024: BG2PC
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000026: BG2PD
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000028: BG2X_LO
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400002A: BG2X_HI
	s_registers.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x0400002C: BG2Y_LO
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400002E: BG2Y_HI
	s_registers.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x04000030: BG3PA
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000032: BG3PB
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000034: BG3PC
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000036: BG3PD
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part"), 8, 8 },
	});
	// 0x04000038: BG3X_LO
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400003A: BG3X_HI
	s_registers.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x0400003C: BG3Y_LO
	s_registers.append({
		{ tr("Fractional part"), 0, 8 },
		{ tr("Integer part (bottom)"), 8, 8 },
	});
	// 0x0400003E: BG3Y_HI
	s_registers.append({
		{ tr("Integer part (top)"), 0, 12 },
	});
	// 0x04000040: WIN0H
	s_registers.append({
		{ tr("End x"), 0, 8 },
		{ tr("Start x"), 8, 8 },
	});
	// 0x04000042: WIN1H
	s_registers.append({
		{ tr("End x"), 0, 8 },
		{ tr("Start x"), 8, 8 },
	});
	// 0x04000044: WIN0V
	s_registers.append({
		{ tr("End y"), 0, 8 },
		{ tr("Start y"), 8, 8 },
	});
	// 0x04000046: WIN1V
	s_registers.append({
		{ tr("End y"), 0, 8 },
		{ tr("Start y"), 8, 8 },
	});
	// 0x04000048: WININ
	s_registers.append({
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
	s_registers.append({
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
	s_registers.append({
		{ tr("Background mosaic size vertical"), 0, 4 },
		{ tr("Background mosaic size horizontal"), 4, 4 },
		{ tr("Object mosaic size vertical"), 8, 4 },
		{ tr("Object mosaic size horizontal"), 12, 4 },
	});
	// 0x0400004E: Unused
	s_registers.append(RegisterDescription());
	// 0x04000050: BLDCNT
	s_registers.append({
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
	s_registers.append({
		{ tr("Blend A (target 1)"), 0, 5 },
		{ tr("Blend B (target 2)"), 8, 5 },
	});
	// 0x04000054: BLDY
	s_registers.append({
		{ tr("Blend Y"), 0, 5 },
	});
	// 0x04000056: Unused
	s_registers.append(RegisterDescription());
	// 0x04000058: Unused
	s_registers.append(RegisterDescription());
	// 0x0400005A: Unused
	s_registers.append(RegisterDescription());
	// 0x0400005C: Unused
	s_registers.append(RegisterDescription());
	// 0x0400005E: Unused
	s_registers.append(RegisterDescription());
	// 0x04000060: SOUND1CNT_LO
	s_registers.append({
		{ tr("Sweep shifts"), 0, 3 },
		{ tr("Sweep subtract"), 3 },
		{ tr("Sweep time (in 1/128s)"), 4, 3 },
	});
	// 0x04000062: SOUND1CNT_HI
	s_registers.append({
		{ tr("Sound length"), 0, 6 },
		{ tr("Duty cycle"),  6, 2 },
		{ tr("Envelope step time"), 8, 3 },
		{ tr("Envelope increase"), 11 },
		{ tr("Initial volume"), 12, 4 },
	});
	// 0x04000064: SOUND1CNT_X
	s_registers.append({
		{ tr("Sound frequency"), 0, 11 },
		{ tr("Timed"),  14 },
		{ tr("Reset"), 15 },
	});
	// 0x04000066: Unused
	s_registers.append(RegisterDescription());
	// 0x04000068: SOUND2CNT_LO
	s_registers.append({
		{ tr("Sound length"), 0, 6 },
		{ tr("Duty cycle"),  6, 2 },
		{ tr("Envelope step time"), 8, 3 },
		{ tr("Envelope increase"), 11 },
		{ tr("Initial volume"), 12, 4 },
	});
	// 0x0400006A: Unused
	s_registers.append(RegisterDescription());
	// 0x0400006C: SOUND2CNT_HI
	s_registers.append({
		{ tr("Sound frequency"), 0, 11 },
		{ tr("Timed"),  14 },
		{ tr("Reset"), 15 },
	});
	// 0x0400006E: Unused
	s_registers.append(RegisterDescription());
	// 0x04000070: SOUND3CNT_LO
	s_registers.append({
		{ tr("Double-size wave table"), 5 },
		{ tr("Active wave table"),  6 },
		{ tr("Enable channel 3"), 7 },
	});
	// 0x04000072: SOUND3CNT_HI
	s_registers.append({
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
	s_registers.append({
		{ tr("Sound frequency"), 0, 11 },
		{ tr("Timed"),  14 },
		{ tr("Reset"), 15 },
	});
	// 0x04000076: Unused
	s_registers.append(RegisterDescription());
	// 0x04000078: SOUND4CNT_LO
	s_registers.append({
		{ tr("Sound length"), 0, 6 },
		{ tr("Envelope step time"), 8, 3 },
		{ tr("Envelope increase"), 11 },
		{ tr("Initial volume"), 12, 4 },
	});
	// 0x0400007A: Unused
	s_registers.append(RegisterDescription());
	// 0x0400007C: SOUND4CNT_HI
	s_registers.append({
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
	s_registers.append(RegisterDescription());
	// 0x04000080: SOUNDCNT_LO
	s_registers.append({
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
	s_registers.append({
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
	s_registers.append({
		{ tr("Active channel 1"), 0, 1, true },
		{ tr("Active channel 2"), 1, 1, true },
		{ tr("Active channel 3"), 2, 1, true },
		{ tr("Active channel 4"), 3, 1, true },
		{ tr("Enable audio"), 7 },
	});
	// 0x04000086: Unused
	s_registers.append(RegisterDescription());
	// 0x04000088: SOUNDBIAS
	s_registers.append({
		{ tr("Bias"), 0, 10 },
		{ tr("Resolution"), 14, 2 },
	});
	// 0x0400008A: Unused
	s_registers.append(RegisterDescription());
	// 0x0400008C: Unused
	s_registers.append(RegisterDescription());
	// 0x0400008E: Unused
	s_registers.append(RegisterDescription());
	// 0x04000090: WAVE_RAM0_LO
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000092: WAVE_RAM0_HI
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000094: WAVE_RAM1_LO
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000096: WAVE_RAM1_HI
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x04000098: WAVE_RAM2_LO
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x0400009A: WAVE_RAM2_HI
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x0400009C: WAVE_RAM3_LO
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x0400009E: WAVE_RAM0_HI
	s_registers.append({
		{ tr("Sample"), 0, 4 },
		{ tr("Sample"), 4, 4 },
		{ tr("Sample"), 8, 4 },
		{ tr("Sample"), 12, 4 },
	});
	// 0x040000A0: FIFO_A_LO
	s_registers.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A2: FIFO_A_HI
	s_registers.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A4: FIFO_B_LO
	s_registers.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A6: FIFO_B_HI
	s_registers.append({
		{ tr("Sample"), 0, 8 },
		{ tr("Sample"), 8, 8 },
	});
	// 0x040000A8: Unused
	s_registers.append(RegisterDescription());
	// 0x040000AA: Unused
	s_registers.append(RegisterDescription());
	// 0x040000AC: Unused
	s_registers.append(RegisterDescription());
	// 0x040000AE: Unused
	s_registers.append(RegisterDescription());
	// 0x040000B0: DMA0SAD_LO
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000B2: DMA0SAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000B4: DMA0DAD_LO
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000B6: DMA0DAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000B8: DMA0CNT_LO
	s_registers.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000BA: DMA0CNT_HI
	s_registers.append({
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
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000BE: DMA1SAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000C0: DMA1DAD_LO
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000C2: DMA1DAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000C4: DMA1CNT_LO
	s_registers.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000C6: DMA1CNT_HI
	s_registers.append({
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
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000CA: DMA2SAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000CC: DMA2DAD_LO
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000CE: DMA2DAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000D0: DMA2CNT_LO
	s_registers.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000D2: DMA2CNT_HI
	s_registers.append({
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
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000D6: DMA3SAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000D8: DMA3DAD_LO
	s_registers.append({
		{ tr("Address (bottom)"), 0, 16 },
	});
	// 0x040000DA: DMA3DAD_HI
	s_registers.append({
		{ tr("Address (top)"), 0, 16 },
	});
	// 0x040000DC: DMA3CNT_LO
	s_registers.append({
		{ tr("Word count"), 0, 16 },
	});
	// 0x040000DE: DMA3CNT_HI
	s_registers.append({
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
	return s_registers;
}

IOViewer::IOViewer(GameController* controller, QWidget* parent)
	: QDialog(parent)
	, m_controller(controller)
{
	m_ui.setupUi(this);

	for (unsigned i = 0; i < REG_MAX >> 1; ++i) {
		const char* reg = GBAIORegisterNames[i];
		if (!reg) {
			continue;
		}
		m_ui.regSelect->addItem("0x0400" + QString("%1: %2").arg(i << 1, 4, 16, QChar('0')).toUpper().arg(reg), i << 1);
	}

	const QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	m_ui.regValue->setFont(font);

	connect(m_ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonPressed(QAbstractButton*)));
	connect(m_ui.buttonBox, SIGNAL(rejected()), this, SLOT(close()));
	connect(m_ui.regSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(selectRegister()));

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

	for (int i = 0; i < 16; ++i) {
		connect(m_b[i], SIGNAL(toggled(bool)), this, SLOT(bitFlipped()));
	}

	selectRegister(0);
}

void IOViewer::updateRegister() {
	m_value = 0;
	uint16_t value = 0;
	m_controller->threadInterrupt();
	if (m_controller->isLoaded()) {
		value = GBAIORead(m_controller->thread()->gba, m_register);
	}
	m_controller->threadContinue();

	for (int i = 0; i < 16; ++i) {
		m_b[i]->setChecked(value & (1 << i) ? Qt::Checked : Qt::Unchecked);
	}
	m_value = value;
	emit valueChanged();
}

void IOViewer::bitFlipped() {
	m_value = 0;
	for (int i = 0; i < 16; ++i) {
		m_value |= m_b[i]->isChecked() << i;
	}
	m_ui.regValue->setText("0x" + QString("%1").arg(m_value, 4, 16, QChar('0')).toUpper());
	emit valueChanged();
}

void IOViewer::writeback() {
	m_controller->threadInterrupt();
	if (m_controller->isLoaded()) {
		GBAIOWrite(m_controller->thread()->gba, m_register, m_value);
	}
	m_controller->threadContinue();
	updateRegister();
}

void IOViewer::selectRegister(unsigned address) {
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
	if (registerDescriptions().count() > address >> 1) {
		// TODO: Remove the check when done filling in register information
		const RegisterDescription& description = registerDescriptions().at(address >> 1);
		int i = 0;
		for (const RegisterItem& ri : description) {
			QLabel* label = new QLabel(ri.description);
			box->addWidget(label, i, 0);
			if (ri.size == 1) {
				QCheckBox* check = new QCheckBox;
				check->setEnabled(!ri.readonly);
				box->addWidget(check, i, 1, Qt::AlignRight);
				connect(check, SIGNAL(toggled(bool)), m_b[ri.start], SLOT(setChecked(bool)));
				connect(m_b[ri.start], SIGNAL(toggled(bool)), check, SLOT(setChecked(bool)));
			} else if (ri.items.empty()) {
				QSpinBox* sbox = new QSpinBox;
				sbox->setEnabled(!ri.readonly);
				sbox->setMaximum((1 << ri.size) - 1);
				box->addWidget(sbox, i, 1, Qt::AlignRight);
				auto connection = connect(this, &IOViewer::valueChanged, [sbox, &ri, this]() {
					int v = (m_value >> ri.start) & ((1 << ri.size) - 1);
					bool signalsBlocked = sbox->blockSignals(true);
					sbox->setValue(v);
					sbox->blockSignals(signalsBlocked);
				});
				connect(sbox, &QObject::destroyed, [connection, this]() {
					this->disconnect(connection);
				});
			} else {
				QButtonGroup* group = new QButtonGroup(box);
				group->setExclusive(true);
				for (int o = 0; o < 1 << ri.size; ++o) {
					if (ri.items.at(o).isNull()) {
						continue;
					}
					++i;
					QRadioButton* button = new QRadioButton(ri.items.at(o));
					button->setEnabled(!ri.readonly);
					box->addWidget(button, i, 0, 1, 2, Qt::AlignLeft);
					group->addButton(button, o);
				}

				auto connection = connect(this, &IOViewer::valueChanged, [group, this, &ri]() {
					unsigned v = (m_value >> ri.start) & ((1 << ri.size) - 1);
					for (int i = 0; i < 1 << ri.size; ++i) {
						QAbstractButton* button = group->button(i);
						if (!button) {
							continue;
						}
						bool signalsBlocked = button->blockSignals(true);
						button->setChecked(i == v);
						button->blockSignals(signalsBlocked);
					}
				});
				connect(group, &QObject::destroyed, [connection, this]() {
					this->disconnect(connection);
				});

			}
			++i;
		}
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
