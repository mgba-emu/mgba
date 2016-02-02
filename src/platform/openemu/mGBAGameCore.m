/*
 Copyright (c) 2016, Jeffrey Pfau

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ''AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#import "mGBAGameCore.h"

#include "util/common.h"

#include "gba/cheats.h"
#include "gba/cheats/gameshark.h"
#include "gba/renderers/video-software.h"
#include "gba/serialize.h"
#include "gba/context/context.h"
#include "util/circle-buffer.h"
#include "util/memory.h"
#include "util/vfs.h"

#import <OpenEmuBase/OERingBuffer.h>
#import "OEGBASystemResponderClient.h"
#import <OpenGL/gl.h>

#define SAMPLES 1024

@interface mGBAGameCore () <OEGBASystemResponderClient>
{
	struct GBAContext context;
	struct GBAVideoSoftwareRenderer renderer;
	struct GBACheatDevice cheats;
	NSMutableDictionary *cheatSets;
	uint16_t keys;
}
@end

@implementation mGBAGameCore

- (id)init
{
	if ((self = [super init]))
	{
		// TODO: Add a log handler
		GBAContextInit(&context, 0);
		struct GBAOptions opts = {
			.useBios = true,
			.idleOptimization = IDLE_LOOP_REMOVE
		};
		mCoreConfigLoadDefaults(&context.config, &opts);
		GBAVideoSoftwareRendererCreate(&renderer);
		renderer.outputBuffer = malloc(256 * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
		renderer.outputBufferStride = 256;
		context.renderer = &renderer.d;
		GBAAudioResizeBuffer(&context.gba->audio, SAMPLES);
		GBACheatDeviceCreate(&cheats);
		GBACheatAttachDevice(context.gba, &cheats);
		cheatSets = [[NSMutableDictionary alloc] init];
		keys = 0;
	}

	return self;
}

- (void)dealloc
{
	GBAContextDeinit(&context);
	[cheatSets enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
		UNUSED(key);
		UNUSED(stop);
		GBACheatRemoveSet(&cheats, [obj pointerValue]);
	}];
	GBACheatDeviceDestroy(&cheats);
	[cheatSets enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
		UNUSED(key);
		UNUSED(stop);
		GBACheatSetDeinit([obj pointerValue]);
	}];
	[cheatSets release];
	free(renderer.outputBuffer);

	[super dealloc];
}

#pragma mark - Execution

- (BOOL)loadFileAtPath:(NSString *)path error:(NSError **)error
{
	NSString *batterySavesDirectory = [self batterySavesDirectoryPath];
	[[NSFileManager defaultManager] createDirectoryAtURL:[NSURL fileURLWithPath:batterySavesDirectory]
	                                withIntermediateDirectories:YES
	                                attributes:nil
	                                error:nil];
	if (context.dirs.save) {
		context.dirs.save->close(context.dirs.save);
	}
	context.dirs.save = VDirOpen([batterySavesDirectory UTF8String]);

	if (!GBAContextLoadROM(&context, [path UTF8String], true)) {
		*error = [NSError errorWithDomain:OEGameCoreErrorDomain code:OEGameCoreCouldNotLoadROMError userInfo:nil];
		return NO;
	}

	if (!GBAContextStart(&context)) {
		*error = [NSError errorWithDomain:OEGameCoreErrorDomain code:OEGameCoreCouldNotStartCoreError userInfo:nil];
		return NO;
	}
	return YES;
}

- (void)executeFrame
{
	GBAContextFrame(&context, keys);

	int16_t samples[SAMPLES * 2];
	size_t available = 0;
	available = blip_samples_avail(context.gba->audio.psg.left);
	blip_read_samples(context.gba->audio.psg.left, samples, available, true);
	blip_read_samples(context.gba->audio.psg.right, samples + 1, available, true);
	[[self ringBufferAtIndex:0] write:samples maxLength:available * 4];
}

- (void)resetEmulation
{
	ARMReset(context.cpu);
}

- (void)stopEmulation
{
	GBAContextStop(&context);
	[super stopEmulation];
}

- (void)setupEmulation
{
	blip_set_rates(context.gba->audio.psg.left,  GBA_ARM7TDMI_FREQUENCY, 32768);
	blip_set_rates(context.gba->audio.psg.right, GBA_ARM7TDMI_FREQUENCY, 32768);
}

#pragma mark - Video

- (OEIntSize)aspectSize
{
	return OEIntSizeMake(3, 2);
}

- (OEIntRect)screenRect
{
    return OEIntRectMake(0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
}

- (OEIntSize)bufferSize
{
    return OEIntSizeMake(256, VIDEO_VERTICAL_PIXELS);
}

- (const void *)videoBuffer
{
	return renderer.outputBuffer;
}

- (GLenum)pixelFormat
{
    return GL_RGBA;
}

- (GLenum)pixelType
{
    return GL_UNSIGNED_INT_8_8_8_8_REV;
}

- (GLenum)internalPixelFormat
{
    return GL_RGB8;
}

- (NSTimeInterval)frameInterval
{
	return GBA_ARM7TDMI_FREQUENCY / (double) VIDEO_TOTAL_LENGTH;
}

#pragma mark - Audio

- (NSUInteger)channelCount
{
    return 2;
}

- (double)audioSampleRate
{
    return 32768;
}

#pragma mark - Save State

- (NSData *)serializeStateWithError:(NSError **)outError
{
	struct VFile* vf = VFileMemChunk(nil, 0);
	if (!GBASaveStateNamed(context.gba, vf, SAVESTATE_SAVEDATA)) {
		*outError = [NSError errorWithDomain:OEGameCoreErrorDomain code:OEGameCoreCouldNotLoadStateError userInfo:nil];
		vf->close(vf);
		return nil;
	}
	size_t size = vf->size(vf);
	void* data = vf->map(vf, size, MAP_READ);
	NSData *nsdata = [NSData dataWithBytes:data length:size];
	vf->unmap(vf, data, size);
	vf->close(vf);
	return nsdata;
}

- (BOOL)deserializeState:(NSData *)state withError:(NSError **)outError
{
	struct VFile* vf = VFileFromConstMemory(state.bytes, state.length);
	if (!GBALoadStateNamed(context.gba, vf, SAVESTATE_SAVEDATA)) {
		*outError = [NSError errorWithDomain:OEGameCoreErrorDomain code:OEGameCoreCouldNotLoadStateError userInfo:nil];
		vf->close(vf);
		return NO;
	}
	vf->close(vf);
	return YES;
}

- (void)saveStateToFileAtPath:(NSString *)fileName completionHandler:(void (^)(BOOL, NSError *))block
{
	struct VFile* vf = VFileOpen([fileName UTF8String], O_CREAT | O_TRUNC | O_RDWR);
	block(GBASaveStateNamed(context.gba, vf, 0), nil);
	vf->close(vf);
}

- (void)loadStateFromFileAtPath:(NSString *)fileName completionHandler:(void (^)(BOOL, NSError *))block
{
	struct VFile* vf = VFileOpen([fileName UTF8String], O_RDONLY);
	block(GBALoadStateNamed(context.gba, vf, 0), nil);
	vf->close(vf);
}

#pragma mark - Input

const int GBAMap[] = {
	GBA_KEY_UP,
	GBA_KEY_DOWN,
	GBA_KEY_LEFT,
	GBA_KEY_RIGHT,
	GBA_KEY_A,
	GBA_KEY_B,
	GBA_KEY_L,
	GBA_KEY_R,
	GBA_KEY_START,
	GBA_KEY_SELECT
};

- (oneway void)didPushGBAButton:(OEGBAButton)button forPlayer:(NSUInteger)player
{
	UNUSED(player);
	keys |= 1 << GBAMap[button];
}

- (oneway void)didReleaseGBAButton:(OEGBAButton)button forPlayer:(NSUInteger)player
{
	UNUSED(player);
	keys &= ~(1 << GBAMap[button]);
}

#pragma mark - Cheats

- (void)setCheat:(NSString *)code setType:(NSString *)type setEnabled:(BOOL)enabled
{
	code = [code stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	code = [code stringByReplacingOccurrencesOfString:@" " withString:@""];

	NSString *codeId = [code stringByAppendingFormat:@"/%@", type];
	struct GBACheatSet* cheatSet = [[cheatSets objectForKey:codeId] pointerValue];
	if (cheatSet) {
		cheatSet->enabled = enabled;
		return;
	}
	cheatSet = malloc(sizeof(*cheatSet));
	GBACheatSetInit(cheatSet, [codeId UTF8String]);
	if ([type isEqual:@"GameShark"]) {
		GBACheatSetGameSharkVersion(cheatSet, 1);
	} else if ([type isEqual:@"Action Replay"]) {
		GBACheatSetGameSharkVersion(cheatSet, 3);
	}
	NSArray *codeSet = [code componentsSeparatedByString:@"+"];
	for (id c in codeSet) {
		GBACheatAddLine(cheatSet, [c UTF8String]);
	}
	cheatSet->enabled = enabled;
	[cheatSets setObject:[NSValue valueWithPointer:cheatSet] forKey:codeId];
	GBACheatAddSet(&cheats, cheatSet);
}
@end

