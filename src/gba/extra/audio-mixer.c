/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/extra/audio-mixer.h>

#include <mgba/core/blip_buf.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/video.h>

#define OVERSAMPLE 2

static void _mp2kInit(void* cpu, struct mCPUComponent* component);
static void _mp2kDeinit(struct mCPUComponent* component);

static bool _mp2kEngage(struct GBAAudioMixer* mixer, uint32_t address);
static void _mp2kVblank(struct GBAAudioMixer* mixer);
static void _mp2kStep(struct GBAAudioMixer* mixer);

void GBAAudioMixerCreate(struct GBAAudioMixer* mixer) {
	mixer->d.init = _mp2kInit;
	mixer->d.deinit = _mp2kDeinit;
	mixer->engage = _mp2kEngage;
	mixer->vblank = _mp2kVblank;
	mixer->step = _mp2kStep;
}

void _mp2kInit(void* cpu, struct mCPUComponent* component) {
	struct ARMCore* arm = cpu;
	struct GBA* gba = (struct GBA*) arm->master;
	struct GBAAudioMixer* mixer = (struct GBAAudioMixer*) component;
	gba->audio.mixer = mixer;
	mixer->p = &gba->audio;
	mixer->contextAddress = 0;
	mixer->tempo = 120.0 / 75.0;
	mixer->frame = 0;
	mixer->last.left = 0;
	mixer->last.right = 0;
	memset(&mixer->context, 0, sizeof(mixer->context));
	memset(&mixer->activeTracks, 0, sizeof(mixer->activeTracks));

	size_t i;
	for (i = 0; i < MP2K_MAX_SOUND_CHANNELS; ++i) {
		mixer->activeTracks[i].channel = &mixer->context.chans[i];
		CircleBufferInit(&mixer->activeTracks[i].buffer, 0x10000);
	}
}

void _mp2kDeinit(struct mCPUComponent* component) {
	struct GBAAudioMixer* mixer = (struct GBAAudioMixer*) component;
	size_t i;
	for (i = 0; i < MP2K_MAX_SOUND_CHANNELS; ++i) {
		CircleBufferDeinit(&mixer->activeTracks[i].buffer);
	}
}

static void _loadInstrument(struct ARMCore* cpu, struct GBAMP2kInstrument* instrument, uint32_t base) {
	struct ARMMemory* memory = &cpu->memory;
	instrument->type = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, type), 0);
	instrument->key = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, key), 0);
	instrument->length = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, length), 0);
	instrument->ps.pan = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, ps.pan), 0);
	if (instrument->type == 0x40 || instrument->type == 0x80) {
		instrument->data.subTable = memory->load32(cpu, base + offsetof(struct GBAMP2kInstrument, data.subTable), 0);
		instrument->extInfo.map = memory->load32(cpu, base + offsetof(struct GBAMP2kInstrument, extInfo.map), 0);
	} else {
		instrument->data.waveData = memory->load32(cpu, base + offsetof(struct GBAMP2kInstrument, data.waveData), 0);
		instrument->extInfo.adsr.attack = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, extInfo.adsr.attack), 0);
		instrument->extInfo.adsr.decay = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, extInfo.adsr.decay), 0);
		instrument->extInfo.adsr.sustain = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, extInfo.adsr.sustain), 0);
		instrument->extInfo.adsr.release = memory->load8(cpu, base + offsetof(struct GBAMP2kInstrument, extInfo.adsr.release), 0);
	}
}

static void _lookupInstrument(struct ARMCore* cpu, struct GBAMP2kInstrument* instrument, uint8_t key) {
	struct ARMMemory* memory = &cpu->memory;
	if (instrument->type == 0x40) {
		uint32_t subInstrumentBase = instrument->data.subTable;
		uint32_t keyTable = instrument->extInfo.map;
		uint8_t id = memory->load8(cpu, keyTable + key, 0);
		subInstrumentBase += 12 * id;
		_loadInstrument(cpu, instrument, subInstrumentBase);
	}
	if (instrument->type == 0x80) {
		uint32_t subInstrumentBase = instrument->data.subTable;
		subInstrumentBase += 12 * key;
		_loadInstrument(cpu, instrument, subInstrumentBase);
	}
}

static void _stepSample(struct GBAAudioMixer* mixer, struct GBAMP2kTrack* track) {
	struct ARMCore* cpu = mixer->p->p->cpu;
	struct ARMMemory* memory = &cpu->memory;
	uint32_t headerAddress;
	struct GBAMP2kInstrument instrument = track->track.instrument;

	uint8_t note = track->track.key;
	_lookupInstrument(cpu, &instrument, note);
	double freq;

	switch (instrument.type) {
	case 0x00:
	case 0x08:
	case 0x40:
	case 0x80:
		freq = GBA_ARM7TDMI_FREQUENCY / (double) track->channel->freq;
		break;
	default:
		// We don't care about PSG channels
		return;
	}
	headerAddress = instrument.data.waveData;
	if (headerAddress < 0x20) {
		mLOG(GBA_AUDIO, ERROR, "Audio track has invalid instrument");
		return;
	}
	uint32_t loopOffset = memory->load32(cpu, headerAddress + 0x8, 0);
	uint32_t endOffset = memory->load32(cpu, headerAddress + 0xC, 0);
	uint32_t sampleBase = headerAddress + 0x10;
	uint32_t sampleI = track->samplePlaying;
	double sampleOffset = track->currentOffset;
	double updates = VIDEO_TOTAL_LENGTH / (mixer->tempo * mixer->p->sampleInterval / OVERSAMPLE);
	int nSample;
	for (nSample = 0; nSample < updates; ++nSample) {
		int8_t sample = memory->load8(cpu, sampleBase + sampleI, 0);

		struct mStereoSample stereo = {
			(sample * track->channel->leftVolume * track->channel->envelopeV) >> 9,
			(sample * track->channel->rightVolume * track->channel->envelopeV) >> 9
		};

		CircleBufferWrite16(&track->buffer, stereo.left);
		CircleBufferWrite16(&track->buffer, stereo.right);

		sampleOffset += mixer->p->sampleInterval / OVERSAMPLE;
		while (sampleOffset > freq) {
			sampleOffset -= freq;
			++sampleI;
			if (sampleI >= endOffset) {
				sampleI = loopOffset;
			}
		}
	}

	track->samplePlaying = sampleI;
	track->currentOffset = sampleOffset;
}

static void _mp2kReload(struct GBAAudioMixer* mixer) {
	struct ARMCore* cpu = mixer->p->p->cpu;
	struct ARMMemory* memory = &cpu->memory;
	mixer->context.magic = memory->load32(cpu, mixer->contextAddress + offsetof(struct GBAMP2kContext, magic), 0);
	int i;
	for (i = 0; i < MP2K_MAX_SOUND_CHANNELS; ++i) {
		struct GBAMP2kSoundChannel* ch = &mixer->context.chans[i];
		struct GBAMP2kTrack* track = &mixer->activeTracks[i];
		track->waiting = false;
		uint32_t base = mixer->contextAddress + offsetof(struct GBAMP2kContext, chans[i]);

		ch->status = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, status), 0);
		ch->type = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, type), 0);
		ch->rightVolume = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, rightVolume), 0);
		ch->leftVolume = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, leftVolume), 0);
		ch->adsr.attack = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, adsr.attack), 0);
		ch->adsr.decay = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, adsr.decay), 0);
		ch->adsr.sustain = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, adsr.sustain), 0);
		ch->adsr.release = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, adsr.release), 0);
		ch->ky = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, ky), 0);
		ch->envelopeV = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, envelopeV), 0);
		ch->envelopeRight = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, envelopeRight), 0);
		ch->envelopeLeft = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, envelopeLeft), 0);
		ch->echoVolume = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, echoVolume), 0);
		ch->echoLength = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, echoLength), 0);
		ch->d1 = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, d1), 0);
		ch->d2 = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, d2), 0);
		ch->gt = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, gt), 0);
		ch->midiKey = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, midiKey), 0);
		ch->ve = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, ve), 0);
		ch->pr = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, pr), 0);
		ch->rp = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, rp), 0);
		ch->d3[0] = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, d3[0]), 0);
		ch->d3[1] = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, d3[1]), 0);
		ch->d3[2] = memory->load8(cpu, base + offsetof(struct GBAMP2kSoundChannel, d3[2]), 0);
		ch->ct = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, ct), 0);
		ch->fw = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, fw), 0);
		ch->freq = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, freq), 0);
		ch->waveData = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, waveData), 0);
		ch->cp = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, cp), 0);
		ch->track = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, track), 0);
		ch->pp = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, pp), 0);
		ch->np = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, np), 0);
		ch->d4 = memory->load32(cpu, base + offsetof(struct GBAMP2kSoundChannel, d4), 0);
		ch->xpi = memory->load16(cpu, base + offsetof(struct GBAMP2kSoundChannel, xpi), 0);
		ch->xpc = memory->load16(cpu, base + offsetof(struct GBAMP2kSoundChannel, xpc), 0);

		base = ch->track;
		if (base) {
			track->track.flags = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, flags), 0);
			track->track.wait = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, wait), 0);
			track->track.patternLevel = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, patternLevel), 0);
			track->track.repN = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, repN), 0);
			track->track.gateTime = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, gateTime), 0);
			track->track.key = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, key), 0);
			track->track.velocity = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, velocity), 0);
			track->track.runningStatus = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, runningStatus), 0);
			track->track.keyM = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, keyM), 0);
			track->track.pitM = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, pitM), 0);
			track->track.keyShift = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, keyShift), 0);
			track->track.keyShiftX = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, keyShiftX), 0);
			track->track.tune = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, tune), 0);
			track->track.pitX = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, pitX), 0);
			track->track.bend = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, bend), 0);
			track->track.bendRange = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, bendRange), 0);
			track->track.volMR = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, volMR), 0);
			track->track.volML = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, volML), 0);
			track->track.vol = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, vol), 0);
			track->track.volX = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, volX), 0);
			track->track.pan = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, pan), 0);
			track->track.panX = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, panX), 0);
			track->track.modM = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, modM), 0);
			track->track.mod = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, mod), 0);
			track->track.modT = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, modT), 0);
			track->track.lfoSpeed = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, lfoSpeed), 0);
			track->track.lfoSpeedC = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, lfoSpeedC), 0);
			track->track.lfoDelay = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, lfoDelay), 0);
			track->track.lfoDelayC = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, lfoDelayC), 0);
			track->track.priority = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, priority), 0);
			track->track.echoVolume = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, echoVolume), 0);
			track->track.echoLength = memory->load8(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, echoLength), 0);
			track->track.chan = memory->load32(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, chan), 0);
			_loadInstrument(cpu, &track->track.instrument, base + offsetof(struct GBAMP2kMusicPlayerTrack, instrument));
			track->track.cmdPtr = memory->load32(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, cmdPtr), 0);
			track->track.patternStack[0] = memory->load32(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, patternStack[0]), 0);
			track->track.patternStack[1] = memory->load32(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, patternStack[1]), 0);
			track->track.patternStack[2] = memory->load32(cpu, base + offsetof(struct GBAMP2kMusicPlayerTrack, patternStack[2]), 0);
		} else {
			memset(&track->track, 0, sizeof(track->track));
		}
		if (track->track.runningStatus == 0xCD) {
			// XCMD isn't supported
			mixer->p->externalMixing = false;
		}
	}
}

bool _mp2kEngage(struct GBAAudioMixer* mixer, uint32_t address) {
	if (address < GBA_BASE_EWRAM) {
		return false;
	}
	if (address != mixer->contextAddress) {
		mixer->contextAddress = address;
		mixer->p->externalMixing = true;
		_mp2kReload(mixer);
	}
	return true;
}

void _mp2kStep(struct GBAAudioMixer* mixer) {
	mixer->frame += mixer->p->sampleInterval;

	while (mixer->frame >= VIDEO_TOTAL_LENGTH / mixer->tempo) {
		int i;
		for (i = 0; i < MP2K_MAX_SOUND_CHANNELS; ++i) {
			struct GBAMP2kTrack* track = &mixer->activeTracks[i];
			if (track->channel->status > 0) {
				_stepSample(mixer, track);
			} else {
				track->currentOffset = 0;
				track->samplePlaying = 0;
				CircleBufferClear(&track->buffer);
			}
		}
		mixer->frame -= VIDEO_TOTAL_LENGTH / mixer->tempo;
	}

	uint32_t interval = mixer->p->sampleInterval / OVERSAMPLE;
	int i;
	for (i = 0; i < OVERSAMPLE; ++i) {
		struct mStereoSample sample = {0};
		size_t track;
		for (track = 0; track < MP2K_MAX_SOUND_CHANNELS; ++track) {
			if (!mixer->activeTracks[track].channel->status) {
				continue;
			}
			int16_t value;
			CircleBufferRead16(&mixer->activeTracks[track].buffer, &value);
			sample.left += value;
			CircleBufferRead16(&mixer->activeTracks[track].buffer, &value);
			sample.right += value;
		}
		sample.left = (sample.left * mixer->p->masterVolume) >> 8;
		sample.right = (sample.right * mixer->p->masterVolume) >> 8;
		if (mixer->p->externalMixing) {
			blip_add_delta(mixer->p->psg.left, mixer->p->clock + i * interval, sample.left - mixer->last.left);
			blip_add_delta(mixer->p->psg.right, mixer->p->clock + i * interval, sample.right - mixer->last.right);
		}
		mixer->last = sample;
	}
}

void _mp2kVblank(struct GBAAudioMixer* mixer) {
	if (!mixer->contextAddress) {
		return;
	}
	mLOG(GBA_AUDIO, DEBUG, "Frame");
	mixer->p->externalMixing = true;
	_mp2kReload(mixer);
}
