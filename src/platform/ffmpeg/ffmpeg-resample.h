/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef FFMPEG_RESAMPLE
#define FFMPEG_RESAMPLE

struct AVAudioResampleContext;
struct GBAAudio;
struct GBAStereoSample;

struct AVAudioResampleContext* GBAAudioOpenLAVR(struct GBAAudio* audio, unsigned outputRate);
unsigned GBAAudioResampleLAVR(struct GBAAudio* audio, struct AVAudioResampleContext* avr, struct GBAStereoSample* output, unsigned nSamples);

#endif
