
#include "platform/sdl/sdl-audio.h"
#include "platform/sdl/sdl-events.h"
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <mgba/core/core.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gb/input.h>
#include <mgba/internal/gba/input.h>

// Info: All this static data live in main.c
// Info: I should probable move everything to a struct and work from there.
static struct mCore* core;
static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Texture* tex;
static float render_scale;
static bool full_stop;
static SDL_Keycode speedupKey;
static int simulationSpeed;
static int fastSimulationSpeed;
static int slowSimulationSpeed;
static SDL_Keycode speedupKey;
static struct mSDLAudio audio;

static void setKey(const char* key, int code) {
	SDL_Keycode sdl_code = SDL_GetKeyFromName(key);
	if (sdl_code == SDLK_UNKNOWN)
		return;
	if (core)
		mInputBindKey(&core->inputMap, SDL_BINDING_KEY, sdl_code, code);
}

static const char* getKey(int input) {
	int code = mInputQueryBinding(&core->inputMap, SDL_BINDING_KEY, input);
	return SDL_GetKeyName(code);
}

// Other Keys
EMSCRIPTEN_KEEPALIVE const char* getKeySpeedToggle() {
	return SDL_GetKeyName(speedupKey);
}
EMSCRIPTEN_KEEPALIVE void setKeySpeedToggle(const char* key) {
	SDL_Keycode sdl_code = SDL_GetKeyFromName(key);
	if (sdl_code == SDLK_UNKNOWN)
		return;
	speedupKey = sdl_code;
}
void EMSCRIPTEN_KEEPALIVE mute() {
	mSDLPauseAudio(&audio);
}

void EMSCRIPTEN_KEEPALIVE unMute() {
	mSDLResumeAudio(&audio);
}

EMSCRIPTEN_KEEPALIVE void setVolume(float vol) {
	if (vol > 2.0)
		return; // this is a percentage so more than 200% is insane.

	int volume = (int) (vol * 0x100);

	if (volume < 0)
		return;
	else if (volume == 0)
		return mSDLPauseAudio(&audio);
	else {
		mCoreConfigSetDefaultIntValue(&core->config, "volume", volume);
		core->reloadConfigOption(core, "volume", &core->config);
		mSDLResumeAudio(&audio);
	}
}

EMSCRIPTEN_KEEPALIVE const char* getPlatform() {
	if (!core)
		return "Uninitialized";
	switch (core->platform(core)) {
	case mPLATFORM_NONE:
		return "None";
	case mPLATFORM_GBA:
		return "GBA";
	case mPLATFORM_GB:
		return "GB";
	}
}
EMSCRIPTEN_KEEPALIVE void stopGame() {
	full_stop = true;
}

EMSCRIPTEN_KEEPALIVE void startGame() {
	full_stop = false;
}

// Setting the scale that make the rendering window fit inside the box defined while keeping the aspect ration.
EMSCRIPTEN_KEEPALIVE void setSize(float width, float height) {
	unsigned w, h;
	core->desiredVideoDimensions(core, &w, &h);
	float render_scaleW = width / w;
	float render_scaleH = height / h;
	render_scale = render_scaleH < render_scaleW ? render_scaleH : render_scaleW;
	SDL_SetWindowSize(window, w * render_scale, h * render_scale);
	SDL_RenderSetScale(renderer, render_scale, render_scale);
}

EMSCRIPTEN_KEEPALIVE void setSpeed(int speed) {
	fastSimulationSpeed = speed;
}

// Info: This macro generates the functions I want to export to javascript. Not use it anywhere else. they require some
// Info: of the functions created above. Using the macros creates 4 function: getKey##name, setKey##name, press##name,
// Info: release##name

#define CREATE_EXPORTS(key, name)                                                           \
	EMSCRIPTEN_KEEPALIVE const char* getKey##name() { return getKey(GBA_KEY_##key); }       \
	EMSCRIPTEN_KEEPALIVE void setKey##name(const char* key) { setKey(key, GBA_KEY_##key); } \
	EMSCRIPTEN_KEEPALIVE void press##name() { core->addKeys(core, 1 << GBA_KEY_##key); }    \
	EMSCRIPTEN_KEEPALIVE void release##name() { core->clearKeys(core, 1 << GBA_KEY_##key); }
#define CREATE_SIMPLE_EXPORTS(x) CREATE_EXPORTS(x, x)

CREATE_SIMPLE_EXPORTS(A)
CREATE_SIMPLE_EXPORTS(B)
CREATE_SIMPLE_EXPORTS(L)
CREATE_SIMPLE_EXPORTS(R)
CREATE_EXPORTS(START, Start)
CREATE_EXPORTS(SELECT, Select)
CREATE_EXPORTS(UP, Up)
CREATE_EXPORTS(DOWN, Down)
CREATE_EXPORTS(LEFT, Left)
CREATE_EXPORTS(RIGHT, Right)
