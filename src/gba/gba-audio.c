#include "gba-audio.h"

#include "gba.h"

void GBAAudioInit(struct GBAAudio* audio) {
	(void)(audio);
}

void GBAAudioDeinit(struct GBAAudio* audio) {
	(void)(audio);	
}

void GBAAudioWriteSOUND1CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.sweep.packed = value;
}

void GBAAudioWriteSOUND1CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.wave.packed = value;
}

void GBAAudioWriteSOUND1CNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->ch1.control.packed = value;
}

void GBAAudioWriteSOUND2CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch2.wave.packed = value;
}

void GBAAudioWriteSOUND2CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch2.control.packed = value;
}

void GBAAudioWriteSOUND3CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.bank.packed = value;
}

void GBAAudioWriteSOUND3CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.wave.packed = value;
}

void GBAAudioWriteSOUND3CNT_X(struct GBAAudio* audio, uint16_t value) {
	audio->ch3.control.packed = value;
}

void GBAAudioWriteSOUND4CNT_LO(struct GBAAudio* audio, uint16_t value) {
	audio->ch4.wave.packed = value;
}

void GBAAudioWriteSOUND4CNT_HI(struct GBAAudio* audio, uint16_t value) {
	audio->ch4.control.packed = value;
}

void GBAAudioWriteSOUNDCNT_LO(struct GBAAudio* audio, uint16_t value) {
	GBALog(audio->p, GBA_LOG_STUB, "Audio unimplemented");
}

void GBAAudioWriteSOUNDCNT_HI(struct GBAAudio* audio, uint16_t value) {
	GBALog(audio->p, GBA_LOG_STUB, "Audio unimplemented");
}

void GBAAudioWriteSOUNDCNT_X(struct GBAAudio* audio, uint16_t value) {
	GBALog(audio->p, GBA_LOG_STUB, "Audio unimplemented");
}

void GBAAudioWriteWaveRAM(struct GBAAudio* audio, int address, uint32_t value) {
	GBALog(audio->p, GBA_LOG_STUB, "Audio unimplemented");
}

void GBAAudioWriteFIFO(struct GBAAudio* audio, int address, uint32_t value) {
	GBALog(audio->p, GBA_LOG_STUB, "Audio unimplemented");
}

