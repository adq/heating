#ifndef HW_H
#define HW_H 1

#include <stdint.h>

#define RESET_PIN RPI_V2_GPIO_P1_22
#define CS_433MHZ BCM2835_SPI_CS1
#define CS_868MHZ BCM2835_SPI_CS0

void resetRFModules();
void spiMode();
void writeReg(uint8_t addr, uint8_t value);
void writeRegMultibyte(uint8_t addr, uint8_t *values, uint8_t len);
uint8_t readReg(uint8_t addr);
void initHardware();
void shutdownHardware();

#endif
