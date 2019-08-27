#ifndef HEATING_H
#define HEATING_H 1

#include <stdint.h>

void resetRFModules();
void spiMode();
void writeReg(uint8_t addr, uint8_t value);
void writeRegMultibyte(uint8_t addr, uint8_t *values, uint8_t len);
uint8_t readReg(uint8_t addr);

#endif
