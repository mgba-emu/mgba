/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/io.h>

#include <mgba/core/interface.h>
#include <mgba/internal/ds/audio.h>
#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/gx.h>
#include <mgba/internal/ds/ipc.h>
#include <mgba/internal/ds/slot1.h>
#include <mgba/internal/ds/spi.h>

mLOG_DEFINE_CATEGORY(DS_IO, "DS I/O", "ds.io");

static void _DSHaltCNT(struct DSCommon* dscore, uint8_t value) {
	switch (value >> 6) {
	case 0:
	default:
		break;
	case 1:
		mLOG(DS_IO, STUB, "Enter GBA mode not supported");
		break;
	case 2:
		ARMHalt(dscore->cpu);
		break;
	case 3:
		mLOG(DS_IO, STUB, "Enter sleep mode not supported");
		break;
	}
}

static uint16_t _scheduleDiv(struct DS* ds, uint16_t control) {
	mTimingDeschedule(&ds->ds9.timing, &ds->divEvent);
	mTimingSchedule(&ds->ds9.timing, &ds->divEvent, (control & 3) ? 36 : 68);
	return control | 0x8000;
}

static uint16_t _scheduleSqrt(struct DS* ds, uint16_t control) {
	mTimingDeschedule(&ds->ds9.timing, &ds->sqrtEvent);
	mTimingSchedule(&ds->ds9.timing, &ds->sqrtEvent, 26);
	return control | 0x8000;
}

static uint32_t DSIOWrite(struct DSCommon* dscore, uint32_t address, uint16_t value) {
	switch (address) {
	// Video
	case DS_REG_DISPSTAT:
		DSVideoWriteDISPSTAT(dscore, value);
		break;

	// DMA Fill
	case DS_REG_DMA0FILL_LO:
	case DS_REG_DMA0FILL_HI:
	case DS_REG_DMA1FILL_LO:
	case DS_REG_DMA1FILL_HI:
	case DS_REG_DMA2FILL_LO:
	case DS_REG_DMA2FILL_HI:
	case DS_REG_DMA3FILL_LO:
	case DS_REG_DMA3FILL_HI:
		break;

	// Timers
	case DS_REG_TM0CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[0], value);
		return 0x20000;
	case DS_REG_TM1CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[1], value);
		return 0x20000;
	case DS_REG_TM2CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[2], value);
		return 0x20000;
	case DS_REG_TM3CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[3], value);
		return 0x20000;

	case DS_REG_TM0CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[0], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM0CNT_LO >> 1], value);
		break;
	case DS_REG_TM1CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[1], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], value);
		break;
	case DS_REG_TM2CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[2], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], value);
		break;
	case DS_REG_TM3CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[3], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], value);
		break;

	// IPC
	case DS_REG_IPCSYNC:
		value &= 0x6F00;
		value |= dscore->memory.io[address >> 1] & 0x000F;
		DSIPCWriteSYNC(dscore->ipc->cpu, dscore->ipc->memory.io, value);
		break;
	case DS_REG_IPCFIFOCNT:
		value = DSIPCWriteFIFOCNT(dscore, value);
		break;

	// Cart bus
	case DS_REG_AUXSPICNT:
		if (dscore->memory.slot1Access) {
			value = DSSlot1Configure(dscore->p, value);
			dscore->ipc->memory.io[address >> 1] = value;
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
		break;
	case DS_REG_AUXSPIDATA:
		if (dscore->memory.slot1Access) {
			DSSlot1WriteSPI(dscore, value);
			dscore->ipc->memory.io[address >> 1] = value;
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
		break;
	case DS_REG_ROMCNT_HI:
		if (dscore->memory.slot1Access) {
			DSSlot1ROMCNT cnt = value << 16;
			cnt |= dscore->memory.io[(address - 2) >> 1];
			cnt = DSSlot1Control(dscore->p, cnt);
			value = cnt >> 16;
			dscore->ipc->memory.io[address >> 1] = value;
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
		break;
	case DS_REG_ROMCNT_LO:
	case DS_REG_ROMCMD_0:
	case DS_REG_ROMCMD_2:
	case DS_REG_ROMCMD_4:
	case DS_REG_ROMCMD_6:
		if (dscore->memory.slot1Access) {
			dscore->ipc->memory.io[address >> 1] = value;
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
		break;

	// Interrupts
	case DS_REG_IME:
		DSWriteIME(dscore->cpu, dscore->memory.io, value);
		break;
	case 0x20A:
		value = 0;
		// Some bad interrupt libraries will write to this
		break;
	case DS_REG_IF_LO:
	case DS_REG_IF_HI:
		value = dscore->memory.io[address >> 1] & ~value;
		DSGXUpdateGXSTAT(&dscore->p->gx);
		break;
	default:
		return 0;
	}
	return value | 0x10000;
}

uint32_t DSIOWrite32(struct DSCommon* dscore, uint32_t address, uint32_t value) {
	switch (address) {
	case DS_REG_DMA0SAD_LO:
		value = DSDMAWriteSAD(dscore, 0, value);
		break;
	case DS_REG_DMA1SAD_LO:
		value = DSDMAWriteSAD(dscore, 1, value);
		break;
	case DS_REG_DMA2SAD_LO:
		value = DSDMAWriteSAD(dscore, 2, value);
		break;
	case DS_REG_DMA3SAD_LO:
		value = DSDMAWriteSAD(dscore, 3, value);
		break;

	case DS_REG_DMA0DAD_LO:
		value = DSDMAWriteDAD(dscore, 0, value);
		break;
	case DS_REG_DMA1DAD_LO:
		value = DSDMAWriteDAD(dscore, 1, value);
		break;
	case DS_REG_DMA2DAD_LO:
		value = DSDMAWriteDAD(dscore, 2, value);
		break;
	case DS_REG_DMA3DAD_LO:
		value = DSDMAWriteDAD(dscore, 3, value);
		break;

	case DS_REG_IPCFIFOSEND_LO:
		DSIPCWriteFIFO(dscore, value);
		break;
	case DS_REG_IE_LO:
		DSWriteIE(dscore->cpu, dscore->memory.io, value);
		break;
	}

	return value;
}

static uint16_t DSIOReadExKeyInput(struct DS* ds) {
	uint16_t input = 0;
	if (ds->keyCallback) {
		input = ds->keyCallback->readKeys(ds->keyCallback);
	} else if (ds->keySource) {
		input = *ds->keySource;
	}
	input = ~(input >> 10) & 0x3;
	input |= 0x3C;
	input |= ds->memory.io7[DS7_REG_EXTKEYIN >> 1] & 0xC0;
	return input;
}

static uint16_t DSIOReadKeyInput(struct DS* ds) {
	uint16_t input = 0;
	if (ds->keyCallback) {
		input = ds->keyCallback->readKeys(ds->keyCallback);
	} else if (ds->keySource) {
		input = *ds->keySource;
	}
	// TODO: Put back
	/*if (!dscore->p->allowOpposingDirections) {
		unsigned rl = input & 0x030;
		unsigned ud = input & 0x0C0;
		input &= 0x30F;
		if (rl != 0x030) {
			input |= rl;
		}
		if (ud != 0x0C0) {
			input |= ud;
		}
	}*/
	return ~input & 0x3FF;
}

static void DSIOUpdateTimer(struct DSCommon* dscore, uint32_t address) {
	switch (address) {
	case DS_REG_TM0CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[0], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	case DS_REG_TM1CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[1], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	case DS_REG_TM2CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[2], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	case DS_REG_TM3CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[3], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	}
}

void DS7IOInit(struct DS* ds) {
	memset(ds->memory.io7, 0, sizeof(ds->memory.io7));
	ds->memory.io7[DS_REG_IPCFIFOCNT >> 1] = 0x0101;
	ds->memory.io7[DS_REG_POSTFLG >> 1] = 0x0001;
	ds->memory.io7[DS7_REG_EXTKEYIN >> 1] = 0x007F;
}

void DS7IOWrite(struct DS* ds, uint32_t address, uint16_t value) {
	switch (address) {
	case DS7_REG_SPICNT:
		value &= 0xCF83;
		value = DSSPIWriteControl(ds, value);
		break;
	case DS7_REG_SPIDATA:
		DSSPIWrite(ds, value);
		break;
	case DS7_REG_RTC:
		value = DSWriteRTC(ds, value);
		break;
	case DS7_REG_SOUND0CNT_LO:
	case DS7_REG_SOUND1CNT_LO:
	case DS7_REG_SOUND2CNT_LO:
	case DS7_REG_SOUND3CNT_LO:
	case DS7_REG_SOUND4CNT_LO:
	case DS7_REG_SOUND5CNT_LO:
	case DS7_REG_SOUND6CNT_LO:
	case DS7_REG_SOUND7CNT_LO:
	case DS7_REG_SOUND8CNT_LO:
	case DS7_REG_SOUND9CNT_LO:
	case DS7_REG_SOUNDACNT_LO:
	case DS7_REG_SOUNDBCNT_LO:
	case DS7_REG_SOUNDCCNT_LO:
	case DS7_REG_SOUNDDCNT_LO:
	case DS7_REG_SOUNDECNT_LO:
	case DS7_REG_SOUNDFCNT_LO:
		value &= 0x837F;
		DSAudioWriteSOUNDCNT_LO(&ds->audio, (address - DS7_REG_SOUND0CNT_LO) >> 4, value);
		break;
	case DS7_REG_SOUND0CNT_HI:
	case DS7_REG_SOUND1CNT_HI:
	case DS7_REG_SOUND2CNT_HI:
	case DS7_REG_SOUND3CNT_HI:
	case DS7_REG_SOUND4CNT_HI:
	case DS7_REG_SOUND5CNT_HI:
	case DS7_REG_SOUND6CNT_HI:
	case DS7_REG_SOUND7CNT_HI:
	case DS7_REG_SOUND8CNT_HI:
	case DS7_REG_SOUND9CNT_HI:
	case DS7_REG_SOUNDACNT_HI:
	case DS7_REG_SOUNDBCNT_HI:
	case DS7_REG_SOUNDCCNT_HI:
	case DS7_REG_SOUNDDCNT_HI:
	case DS7_REG_SOUNDECNT_HI:
	case DS7_REG_SOUNDFCNT_HI:
		value &= 0xFF7F;
		DSAudioWriteSOUNDCNT_HI(&ds->audio, (address - DS7_REG_SOUND0CNT_HI) >> 4, value);
		break;
	case DS7_REG_SOUND0TMR:
	case DS7_REG_SOUND1TMR:
	case DS7_REG_SOUND2TMR:
	case DS7_REG_SOUND3TMR:
	case DS7_REG_SOUND4TMR:
	case DS7_REG_SOUND5TMR:
	case DS7_REG_SOUND6TMR:
	case DS7_REG_SOUND7TMR:
	case DS7_REG_SOUND8TMR:
	case DS7_REG_SOUND9TMR:
	case DS7_REG_SOUNDATMR:
	case DS7_REG_SOUNDBTMR:
	case DS7_REG_SOUNDCTMR:
	case DS7_REG_SOUNDDTMR:
	case DS7_REG_SOUNDETMR:
	case DS7_REG_SOUNDFTMR:
		DSAudioWriteSOUNDTMR(&ds->audio, (address - DS7_REG_SOUND0TMR) >> 4, value);
		break;
	case DS7_REG_SOUND0PNT:
	case DS7_REG_SOUND1PNT:
	case DS7_REG_SOUND2PNT:
	case DS7_REG_SOUND3PNT:
	case DS7_REG_SOUND4PNT:
	case DS7_REG_SOUND5PNT:
	case DS7_REG_SOUND6PNT:
	case DS7_REG_SOUND7PNT:
	case DS7_REG_SOUND8PNT:
	case DS7_REG_SOUND9PNT:
	case DS7_REG_SOUNDAPNT:
	case DS7_REG_SOUNDBPNT:
	case DS7_REG_SOUNDCPNT:
	case DS7_REG_SOUNDDPNT:
	case DS7_REG_SOUNDEPNT:
	case DS7_REG_SOUNDFPNT:
		DSAudioWriteSOUNDPNT(&ds->audio, (address - DS7_REG_SOUND0PNT) >> 4, value);
		break;
	default:
		{
			uint32_t v2 = DSIOWrite(&ds->ds7, address, value);
			if (v2 & 0x10000) {
				value = v2;
				break;
			} else if (v2 & 0x20000) {
				return;
			}
		}
		if (address >= DS7_IO_BASE_WIFI && address < DS7_IO_END_WIFI) {
			DSWifiWriteIO(ds, address & 0x7FFF, value);
			return;
		}
		mLOG(DS_IO, STUB, "Stub DS7 I/O register write: %06X:%04X", address, value);
		if (address >= DS7_REG_MAX) {
			mLOG(DS_IO, GAME_ERROR, "Write to unused DS7 I/O register: %06X:%04X", address, value);
			return;
		}
		break;
	}
	ds->memory.io7[address >> 1] = value;
}

void DS7IOWrite8(struct DS* ds, uint32_t address, uint8_t value) {
	if (address == DS7_REG_HALTCNT) {
		_DSHaltCNT(&ds->ds7, value);
		return;
	}
	if (address < DS7_REG_MAX) {
		uint16_t value16 = value << (8 * (address & 1));
		value16 |= (ds->ds7.memory.io[(address & 0xFFF) >> 1]) & ~(0xFF << (8 * (address & 1)));
		DS7IOWrite(ds, address & 0xFFFFFFFE, value16);
	} else {
		mLOG(DS, STUB, "Writing to unknown DS7 register: %08X:%02X", address, value);
	}
}

void DS7IOWrite32(struct DS* ds, uint32_t address, uint32_t value) {
	switch (address) {
	case DS_REG_DMA0SAD_LO:
	case DS_REG_DMA1SAD_LO:
	case DS_REG_DMA2SAD_LO:
	case DS_REG_DMA3SAD_LO:
	case DS_REG_DMA0DAD_LO:
	case DS_REG_DMA1DAD_LO:
	case DS_REG_DMA2DAD_LO:
	case DS_REG_DMA3DAD_LO:
	case DS_REG_IPCFIFOSEND_LO:
	case DS_REG_IE_LO:
		value = DSIOWrite32(&ds->ds7, address, value);
		break;

	case DS_REG_DMA0CNT_LO:
		DS7DMAWriteCNT(&ds->ds7, 0, value);
		break;
	case DS_REG_DMA1CNT_LO:
		DS7DMAWriteCNT(&ds->ds7, 1, value);
		break;
	case DS_REG_DMA2CNT_LO:
		DS7DMAWriteCNT(&ds->ds7, 2, value);
		break;
	case DS_REG_DMA3CNT_LO:
		DS7DMAWriteCNT(&ds->ds7, 3, value);
		break;

	case DS7_REG_SOUND0SAD_LO:
	case DS7_REG_SOUND1SAD_LO:
	case DS7_REG_SOUND2SAD_LO:
	case DS7_REG_SOUND3SAD_LO:
	case DS7_REG_SOUND4SAD_LO:
	case DS7_REG_SOUND5SAD_LO:
	case DS7_REG_SOUND6SAD_LO:
	case DS7_REG_SOUND7SAD_LO:
	case DS7_REG_SOUND8SAD_LO:
	case DS7_REG_SOUND9SAD_LO:
	case DS7_REG_SOUNDASAD_LO:
	case DS7_REG_SOUNDBSAD_LO:
	case DS7_REG_SOUNDCSAD_LO:
	case DS7_REG_SOUNDDSAD_LO:
	case DS7_REG_SOUNDESAD_LO:
	case DS7_REG_SOUNDFSAD_LO:
		DSAudioWriteSOUNDSAD(&ds->audio, (address - DS7_REG_SOUND0SAD_LO) >> 4, value);
		break;

	case DS7_REG_SOUND0LEN_LO:
	case DS7_REG_SOUND1LEN_LO:
	case DS7_REG_SOUND2LEN_LO:
	case DS7_REG_SOUND3LEN_LO:
	case DS7_REG_SOUND4LEN_LO:
	case DS7_REG_SOUND5LEN_LO:
	case DS7_REG_SOUND6LEN_LO:
	case DS7_REG_SOUND7LEN_LO:
	case DS7_REG_SOUND8LEN_LO:
	case DS7_REG_SOUND9LEN_LO:
	case DS7_REG_SOUNDALEN_LO:
	case DS7_REG_SOUNDBLEN_LO:
	case DS7_REG_SOUNDCLEN_LO:
	case DS7_REG_SOUNDDLEN_LO:
	case DS7_REG_SOUNDELEN_LO:
	case DS7_REG_SOUNDFLEN_LO:
		value &= 0x3FFFFF;
		DSAudioWriteSOUNDLEN(&ds->audio, (address - DS7_REG_SOUND0LEN_LO) >> 4, value);
		break;

	default:
		DS7IOWrite(ds, address, value & 0xFFFF);
		DS7IOWrite(ds, address | 2, value >> 16);
		return;
	}
	ds->ds7.memory.io[address >> 1] = value;
	ds->ds7.memory.io[(address >> 1) + 1] = value >> 16;
}

uint16_t DS7IORead(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS_REG_TM0CNT_LO:
	case DS_REG_TM1CNT_LO:
	case DS_REG_TM2CNT_LO:
	case DS_REG_TM3CNT_LO:
		DSIOUpdateTimer(&ds->ds7, address);
		break;
	case DS_REG_KEYINPUT:
		return DSIOReadKeyInput(ds);
	case DS7_REG_EXTKEYIN:
		return DSIOReadExKeyInput(ds);
	case DS_REG_VCOUNT:
	case DS_REG_DMA0FILL_LO:
	case DS_REG_DMA0FILL_HI:
	case DS_REG_DMA1FILL_LO:
	case DS_REG_DMA1FILL_HI:
	case DS_REG_DMA2FILL_LO:
	case DS_REG_DMA2FILL_HI:
	case DS_REG_DMA3FILL_LO:
	case DS_REG_DMA3FILL_HI:
	case DS_REG_TM0CNT_HI:
	case DS_REG_TM1CNT_HI:
	case DS_REG_TM2CNT_HI:
	case DS_REG_TM3CNT_HI:
	case DS7_REG_SPICNT:
	case DS7_REG_SPIDATA:
	case DS_REG_IPCSYNC:
	case DS_REG_IPCFIFOCNT:
	case DS_REG_ROMCNT_LO:
	case DS_REG_ROMCNT_HI:
	case DS_REG_IME:
	case 0x20A:
	case DS_REG_IE_LO:
	case DS_REG_IE_HI:
	case DS_REG_IF_LO:
	case DS_REG_IF_HI:
	case DS_REG_POSTFLG:
		// Handled transparently by the registers
		break;
	case DS_REG_AUXSPICNT:
	case DS_REG_AUXSPIDATA:
		if (ds->ds7.memory.slot1Access) {
			break;
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
	default:
		if (address >= DS7_IO_BASE_WIFI && address < DS7_IO_END_WIFI) {
			return DSWifiReadIO(ds, address & 0x7FFF);
		}
		mLOG(DS_IO, STUB, "Stub DS7 I/O register read: %06X", address);
	}
	if (address < DS7_REG_MAX) {
		return ds->memory.io7[address >> 1];
	}

	return 0;
}

uint32_t DS7IORead32(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS_REG_IPCFIFORECV_LO:
		return DSIPCReadFIFO(&ds->ds7);
	case DS_REG_ROMDATA_0:
		if (ds->ds7.memory.slot1Access) {
			return DSSlot1Read(ds);
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
	default:
		return DS7IORead(ds, address & 0x00FFFFFC) | (DS7IORead(ds, (address & 0x00FFFFFC) | 2) << 16);
	}
}

void DS9IOInit(struct DS* ds) {
	memset(ds->memory.io9, 0, sizeof(ds->memory.io9));
	ds->memory.io9[DS_REG_IPCFIFOCNT >> 1] = 0x0101;
	ds->memory.io9[DS_REG_POSTFLG >> 1] = 0x0001;
	ds->memory.io9[DS9_REG_GXSTAT_HI >> 1] = 0x0600;
	DS9IOWrite(ds, DS9_REG_VRAMCNT_G, 0x0300);
}

void DS9IOWrite(struct DS* ds, uint32_t address, uint16_t value) {
	if ((address <= DS9_REG_A_BLDY && address > DS_REG_VCOUNT) || address == DS9_REG_A_DISPCNT_LO || address == DS9_REG_A_DISPCNT_HI || address == DS9_REG_A_MASTER_BRIGHT) {
		value = ds->video.renderer->writeVideoRegister(ds->video.renderer, address, value);
	} else if ((address >= DS9_REG_B_DISPCNT_LO && address <= DS9_REG_B_BLDY) || address == DS9_REG_B_MASTER_BRIGHT) {
		value = ds->video.renderer->writeVideoRegister(ds->video.renderer, address, value);
	} else if ((address >= DS9_REG_RDLINES_COUNT && address <= DS9_REG_VECMTX_RESULT_12) || address == DS9_REG_DISP3DCNT) {
		value = DSGXWriteRegister(&ds->gx, address, value);
	} else {
		uint16_t oldValue;
		switch (address) {
		// Other video
		case DS9_REG_DISPCAPCNT_LO:
			value &= 0x1F1F;
			break;
		case DS9_REG_DISPCAPCNT_HI:
			value &= 0xEF3F;
			break;

		// VRAM control
		case DS9_REG_VRAMCNT_A:
		case DS9_REG_VRAMCNT_C:
		case DS9_REG_VRAMCNT_E:
			oldValue = ds->memory.io9[address >> 1];
			value &= 0x9F9F;
			DSVideoConfigureVRAM(ds, address - DS9_REG_VRAMCNT_A, value & 0xFF, oldValue & 0xFF);
			DSVideoConfigureVRAM(ds, address - DS9_REG_VRAMCNT_A + 1, value >> 8, oldValue >> 8);
			break;
		case DS9_REG_VRAMCNT_G:
			oldValue = ds->memory.io9[address >> 1];
			value &= 0x039F;
			DSVideoConfigureVRAM(ds, 6, value & 0xFF, oldValue & 0xFF);
			DSConfigureWRAM(&ds->memory, value >> 8);
			break;
		case DS9_REG_VRAMCNT_H:
			oldValue = ds->memory.io9[address >> 1];
			value &= 0x9F9F;
			DSVideoConfigureVRAM(ds, 7, value & 0xFF, oldValue & 0xFF);
			DSVideoConfigureVRAM(ds, 8, value >> 8, oldValue >> 8);
			break;

		case DS9_REG_EXMEMCNT:
			value &= 0xE8FF;
			DSConfigureExternalMemory(ds, value);
			break;

		// Math
		case DS9_REG_DIVCNT:
			value = _scheduleDiv(ds, value);
			break;
		case DS9_REG_DIV_NUMER_0:
		case DS9_REG_DIV_NUMER_1:
		case DS9_REG_DIV_NUMER_2:
		case DS9_REG_DIV_NUMER_3:
		case DS9_REG_DIV_DENOM_0:
		case DS9_REG_DIV_DENOM_1:
		case DS9_REG_DIV_DENOM_2:
		case DS9_REG_DIV_DENOM_3:
			ds->memory.io9[DS9_REG_DIVCNT >> 1] = _scheduleDiv(ds, ds->memory.io9[DS9_REG_DIVCNT >> 1]);
			break;
		case DS9_REG_SQRTCNT:
			value = _scheduleSqrt(ds, value);
			break;
		case DS9_REG_SQRT_PARAM_0:
		case DS9_REG_SQRT_PARAM_1:
		case DS9_REG_SQRT_PARAM_2:
		case DS9_REG_SQRT_PARAM_3:
			ds->memory.io9[DS9_REG_SQRTCNT >> 1] = _scheduleSqrt(ds, ds->memory.io9[DS9_REG_SQRTCNT >> 1]);
			break;

		// High Video
		case DS9_REG_POWCNT1:
			value = ds->video.renderer->writeVideoRegister(ds->video.renderer, address, value);
			break;

		default:
			{
				uint32_t v2 = DSIOWrite(&ds->ds9, address, value);
				if (v2 & 0x10000) {
					value = v2;
					break;
				} else if (v2 & 0x20000) {
					return;
				}
			}
			mLOG(DS_IO, STUB, "Stub DS9 I/O register write: %06X:%04X", address, value);
			if (address >= DS7_REG_MAX) {
				mLOG(DS_IO, GAME_ERROR, "Write to unused DS9 I/O register: %06X:%04X", address, value);
				return;
			}
			break;
		}
	}
	ds->memory.io9[address >> 1] = value;
}

void DS9IOWrite8(struct DS* ds, uint32_t address, uint8_t value) {
	if (address < DS9_REG_MAX) {
		uint16_t value16 = value << (8 * (address & 1));
		value16 |= (ds->memory.io9[(address & 0x1FFF) >> 1]) & ~(0xFF << (8 * (address & 1)));
		DS9IOWrite(ds, address & 0xFFFFFFFE, value16);
	} else {
		mLOG(DS, STUB, "Writing to unknown DS9 register: %08X:%02X", address, value);
	}
}

void DS9IOWrite32(struct DS* ds, uint32_t address, uint32_t value) {
	if ((address >= DS9_REG_RDLINES_COUNT && address <= DS9_REG_VECMTX_RESULT_12) || address == DS9_REG_DISP3DCNT) {
		value = DSGXWriteRegister32(&ds->gx, address, value);
	} else {
		switch (address) {
		case DS_REG_DMA0SAD_LO:
		case DS_REG_DMA1SAD_LO:
		case DS_REG_DMA2SAD_LO:
		case DS_REG_DMA3SAD_LO:
		case DS_REG_DMA0DAD_LO:
		case DS_REG_DMA1DAD_LO:
		case DS_REG_DMA2DAD_LO:
		case DS_REG_DMA3DAD_LO:
		case DS_REG_IPCFIFOSEND_LO:
		case DS_REG_IE_LO:
			value = DSIOWrite32(&ds->ds9, address, value);
			break;

		case DS_REG_DMA0CNT_LO:
			DS9DMAWriteCNT(&ds->ds9, 0, value);
			break;
		case DS_REG_DMA1CNT_LO:
			DS9DMAWriteCNT(&ds->ds9, 1, value);
			break;
		case DS_REG_DMA2CNT_LO:
			DS9DMAWriteCNT(&ds->ds9, 2, value);
			break;
		case DS_REG_DMA3CNT_LO:
			DS9DMAWriteCNT(&ds->ds9, 3, value);
			break;

		default:
			DS9IOWrite(ds, address, value & 0xFFFF);
			DS9IOWrite(ds, address | 2, value >> 16);
			return;
		}
	}
	ds->ds9.memory.io[address >> 1] = value;
	ds->ds9.memory.io[(address >> 1) + 1] = value >> 16;
}

uint16_t DS9IORead(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS_REG_TM0CNT_LO:
	case DS_REG_TM1CNT_LO:
	case DS_REG_TM2CNT_LO:
	case DS_REG_TM3CNT_LO:
		DSIOUpdateTimer(&ds->ds9, address);
		break;
	case DS_REG_KEYINPUT:
		return DSIOReadKeyInput(ds);
	case DS_REG_VCOUNT:
	case DS_REG_DMA0FILL_LO:
	case DS_REG_DMA0FILL_HI:
	case DS_REG_DMA1FILL_LO:
	case DS_REG_DMA1FILL_HI:
	case DS_REG_DMA2FILL_LO:
	case DS_REG_DMA2FILL_HI:
	case DS_REG_DMA3FILL_LO:
	case DS_REG_DMA3FILL_HI:
	case DS_REG_TM0CNT_HI:
	case DS_REG_TM1CNT_HI:
	case DS_REG_TM2CNT_HI:
	case DS_REG_TM3CNT_HI:
	case DS_REG_IPCSYNC:
	case DS_REG_IPCFIFOCNT:
	case DS_REG_ROMCNT_LO:
	case DS_REG_ROMCNT_HI:
	case DS_REG_IME:
	case 0x20A:
	case DS_REG_IE_LO:
	case DS_REG_IE_HI:
	case DS_REG_IF_LO:
	case DS_REG_IF_HI:
	case DS9_REG_DIVCNT:
	case DS9_REG_DIV_NUMER_0:
	case DS9_REG_DIV_NUMER_1:
	case DS9_REG_DIV_NUMER_2:
	case DS9_REG_DIV_NUMER_3:
	case DS9_REG_DIV_DENOM_0:
	case DS9_REG_DIV_DENOM_1:
	case DS9_REG_DIV_DENOM_2:
	case DS9_REG_DIV_DENOM_3:
	case DS9_REG_DIV_RESULT_0:
	case DS9_REG_DIV_RESULT_1:
	case DS9_REG_DIV_RESULT_2:
	case DS9_REG_DIV_RESULT_3:
	case DS9_REG_DIVREM_RESULT_0:
	case DS9_REG_DIVREM_RESULT_1:
	case DS9_REG_DIVREM_RESULT_2:
	case DS9_REG_DIVREM_RESULT_3:
	case DS9_REG_SQRTCNT:
	case DS9_REG_SQRT_PARAM_0:
	case DS9_REG_SQRT_PARAM_1:
	case DS9_REG_SQRT_PARAM_2:
	case DS9_REG_SQRT_PARAM_3:
	case DS9_REG_SQRT_RESULT_LO:
	case DS9_REG_SQRT_RESULT_HI:
	case DS_REG_POSTFLG:
	case DS9_REG_GXSTAT_LO:
	case DS9_REG_GXSTAT_HI:
	case DS9_REG_CLIPMTX_RESULT_00:
	case DS9_REG_CLIPMTX_RESULT_01:
	case DS9_REG_CLIPMTX_RESULT_02:
	case DS9_REG_CLIPMTX_RESULT_03:
	case DS9_REG_CLIPMTX_RESULT_04:
	case DS9_REG_CLIPMTX_RESULT_05:
	case DS9_REG_CLIPMTX_RESULT_06:
	case DS9_REG_CLIPMTX_RESULT_07:
	case DS9_REG_CLIPMTX_RESULT_08:
	case DS9_REG_CLIPMTX_RESULT_09:
	case DS9_REG_CLIPMTX_RESULT_0A:
	case DS9_REG_CLIPMTX_RESULT_0B:
	case DS9_REG_CLIPMTX_RESULT_0C:
	case DS9_REG_CLIPMTX_RESULT_0D:
	case DS9_REG_CLIPMTX_RESULT_0E:
	case DS9_REG_CLIPMTX_RESULT_0F:
	case DS9_REG_CLIPMTX_RESULT_10:
	case DS9_REG_CLIPMTX_RESULT_11:
	case DS9_REG_CLIPMTX_RESULT_12:
	case DS9_REG_CLIPMTX_RESULT_13:
	case DS9_REG_CLIPMTX_RESULT_14:
	case DS9_REG_CLIPMTX_RESULT_15:
	case DS9_REG_CLIPMTX_RESULT_16:
	case DS9_REG_CLIPMTX_RESULT_17:
	case DS9_REG_CLIPMTX_RESULT_18:
	case DS9_REG_CLIPMTX_RESULT_19:
	case DS9_REG_CLIPMTX_RESULT_1A:
	case DS9_REG_CLIPMTX_RESULT_1B:
	case DS9_REG_CLIPMTX_RESULT_1C:
	case DS9_REG_CLIPMTX_RESULT_1D:
	case DS9_REG_CLIPMTX_RESULT_1E:
	case DS9_REG_CLIPMTX_RESULT_1F:
		// Handled transparently by the registers
		break;
	case DS_REG_AUXSPICNT:
	case DS_REG_AUXSPIDATA:
		if (ds->ds9.memory.slot1Access) {
			break;
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
	default:
		mLOG(DS_IO, STUB, "Stub DS9 I/O register read: %06X", address);
	}
	if (address < DS9_REG_MAX) {
		return ds->ds9.memory.io[address >> 1];
	}
	return 0;
}

uint32_t DS9IORead32(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS_REG_IPCFIFORECV_LO:
		return DSIPCReadFIFO(&ds->ds9);
	case DS_REG_ROMDATA_0:
		if (ds->ds9.memory.slot1Access) {
			return DSSlot1Read(ds);
		} else {
			mLOG(DS_IO, GAME_ERROR, "Invalid cart access");
			return 0;
		}
	default:
		return DS9IORead(ds, address & 0x00FFFFFC) | (DS9IORead(ds, (address & 0x00FFFFFC) | 2) << 16);
	}
}
