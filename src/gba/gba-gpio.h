#ifndef GBA_GPIO_H
#define GBA_GPIO_H

#include <stdint.h>

#define IS_GPIO_REGISTER(reg) ((reg) == GPIO_REG_DATA || (reg) == GPIO_REG_DIRECTION || (reg) == GPIO_REG_CONTROL)

enum GPIODevice {
	GPIO_NONE = 0,
	GPIO_RTC = 1,
	GPIO_RUMBLE = 2,
	GPIO_LIGHT_SENSOR = 4,
	GPIO_GYRO = 8
};

enum GPIORegister {
	GPIO_REG_DATA = 0xC4,
	GPIO_REG_DIRECTION = 0xC6,
	GPIO_REG_CONTROL = 0xC8
};

enum GPIODirection {
	GPIO_WRITE_ONLY = 0,
	GPIO_READ_WRITE = 1
};

struct GBARTC {
	// TODO
};

struct GBACartridgeGPIO {
	int gpioDevices;
	enum GPIODirection direction;
	uint16_t* gpioBase;

	union {
		struct {
			unsigned p0 : 1;
			unsigned p1 : 1;
			unsigned p2 : 1;
			unsigned p3 : 1;
		};
		uint16_t pinState;
	};

	union {
		struct {
			unsigned dir0 : 1;
			unsigned dir1 : 1;
			unsigned dir2 : 1;
			unsigned dir3 : 1;			
		};
		uint16_t pinDirection;
	};

	struct GBARTC rtc;
};

void GBAGPIOInit(struct GBACartridgeGPIO* gpio, uint16_t* gpioBase);
void GBAGPIOWrite(struct GBACartridgeGPIO* gpio, uint32_t address, uint16_t value);

void GBAGPIOInitRTC(struct GBACartridgeGPIO* gpio);

#endif
