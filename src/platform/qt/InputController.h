#ifndef QGBA_INPUT_CONTROLLER_H
#define QGBA_INPUT_CONTROLLER_H

extern "C" {
#include "gba-input.h"

#ifdef BUILD_SDL
#include "platform/sdl/sdl-events.h"
#endif
}

struct Configuration;

namespace QGBA {

class InputController {
public:
	static const uint32_t KEYBOARD = 0x51545F4B;

	InputController();
	~InputController();

	void loadDefaultConfiguration(const Configuration* config);
	void loadConfiguration(uint32_t type, const Configuration* config);

	GBAKey mapKeyboard(int key) const;

#ifdef BUILD_SDL
	int testSDLEvents();
#endif

private:
	GBAInputMap m_inputMap;

#ifdef BUILD_SDL
	GBASDLEvents m_sdlEvents;
#endif
};

}

#endif
