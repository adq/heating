#ifndef SALUS_H
#define SALUS_H 1

#include <stdint.h>

uint8_t salusChecksum(uint8_t * buf, int buflen);
void txSalusMessage(uint16_t pairingcode, uint8_t salus_v1_command, uint8_t salus_v2_command);
void txSalusPairing(uint16_t pairingcode);
void txSalusBoilerOn(uint16_t pairingcode);
void txSalusBoilerOff(uint16_t pairingcode);

#endif
