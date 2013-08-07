#include <stdint.h>
#include "commands.h"

#define TWI_PREAMBLE 0xff

#define SERVOBOARD_ADDR 0x03
#define SERVOBOARD_I2C_CHARDEV "/dev/i2c-1"

#define LOWBYTE(v) ((unsigned char) (v))
#define HIGHBYTE(v) ((unsigned char)(((unsigned int) (v)) >> 8))

int servoboard_open();
void servoboard_close();
int servoboard_setservo ( uint8_t servoNr, uint16_t servoPos );
int servoboard_setleds ( uint8_t onoff, uint8_t leds );
int servoboard_ping();
