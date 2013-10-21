#include "gba.h"

#include "gba-gpio.h"

static void _readPins(struct GBACartridgeGPIO* gpio);

static void _rtcReadPins(struct GBACartridgeGPIO* gpio);

void GBAGPIOInit(struct GBACartridgeGPIO* gpio, uint16_t* base) {
	gpio->gpioDevices = GPIO_NONE;
	gpio->direction = GPIO_WRITE_ONLY;
	gpio->gpioBase = base;
	gpio->pinState = 0;
	gpio->pinDirection = 0;
}

void GBAGPIOWrite(struct GBACartridgeGPIO* gpio, uint32_t address, uint16_t value) {
	switch (address) {
	case GPIO_REG_DATA:
		gpio->pinState = value;
		_readPins(gpio);
		break;
	case GPIO_REG_DIRECTION:
		gpio->pinDirection = value;
		break;
	case GPIO_REG_CONTROL:
		gpio->direction = value;
		break;
	default:
		GBALog(0, GBA_LOG_WARN, "Invalid GPIO address");
	}
}

void GBAGPIOInitRTC(struct GBACartridgeGPIO* gpio) {
	// TODO
}

void _readPins(struct GBACartridgeGPIO* gpio) {
	if (gpio->gpioDevices & GPIO_RTC) {
		_rtcReadPins(gpio);
	}
}

void _rtcReadPins(struct GBACartridgeGPIO* gpio) {
	// TODO
}
