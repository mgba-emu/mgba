

static void setKey(const char* key, int code) {
	SDL_Keycode sdl_code = SDL_GetKeyFromName(key);
	if(sdl_code != SDLK_UNKNOWN)
	{
		if(core)
			mInputBindKey(&core->inputMap, SDL_BINDING_KEY, sdl_code, code);
	}
}

static const char * getKey(int input) {
	int code = mInputQueryBinding(&core->inputMap, SDL_BINDING_KEY,input);
	printf("KeyCode: %d\n", code);
	return SDL_GetKeyName(code);
}

// Input Getters
// Info: The key mapping are the same for gba and gb so no translation needed.
EMSCRIPTEN_KEEPALIVE const char* getKeyA() {
	return getKey(GBA_KEY_A);
}

EMSCRIPTEN_KEEPALIVE const char* getKeyB() {
	
	return getKey(GBA_KEY_B);
}

EMSCRIPTEN_KEEPALIVE const char* getKeyL() {
	
	return getKey(GBA_KEY_L);
}

EMSCRIPTEN_KEEPALIVE const char* getKeyR() {
	
	return getKey(GBA_KEY_R);
}

EMSCRIPTEN_KEEPALIVE const char* getKeyStart() {
	
	return getKey(GBA_KEY_START);
}

EMSCRIPTEN_KEEPALIVE const char* getKeySelect() {
	
	return getKey(GBA_KEY_SELECT);
}

EMSCRIPTEN_KEEPALIVE const char* getKeyUp() {
	
	return getKey(GBA_KEY_UP);
}

EMSCRIPTEN_KEEPALIVE const char* getKeyDown() {
	
	return getKey(GBA_KEY_DOWN);
}


EMSCRIPTEN_KEEPALIVE const char* getKeyLeft() {
	
	return getKey(GBA_KEY_LEFT);
}

EMSCRIPTEN_KEEPALIVE const char* getKeyRight() {
	
	return getKey(GBA_KEY_RIGHT);
}


// Input Setters
EMSCRIPTEN_KEEPALIVE void setKeyA(const char* key) {
	setKey(key,GBA_KEY_A);
}

EMSCRIPTEN_KEEPALIVE void setKeyB(const char* key) {
	setKey(key,GBA_KEY_B);
}

EMSCRIPTEN_KEEPALIVE void setKeyL(const char* key) {
	setKey(key,GBA_KEY_L);
}

EMSCRIPTEN_KEEPALIVE void setKeyR(const char* key) {
	setKey(key,GBA_KEY_R);
}

EMSCRIPTEN_KEEPALIVE void setKeyStart(const char* key) {
	setKey(key,GBA_KEY_START);
}

EMSCRIPTEN_KEEPALIVE void setKeySelect(const char* key) {
	setKey(key,GBA_KEY_SELECT);
}

EMSCRIPTEN_KEEPALIVE void setKeyUp(const char* key) {
	setKey(key,GBA_KEY_UP);
}

EMSCRIPTEN_KEEPALIVE void setKeyDown(const char* key) {
	setKey(key,GBA_KEY_DOWN);
}


EMSCRIPTEN_KEEPALIVE void setKeyLeft(const char* key) {
	setKey(key,GBA_KEY_LEFT);
}

EMSCRIPTEN_KEEPALIVE void setKeyRight(const char* key) {
	setKey(key,GBA_KEY_RIGHT);
}