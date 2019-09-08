#ifndef SALUS_H
#define SALUS_H 1

#include <stdint.h>

uint8_t salusChecksum(uint8_t * buf, int buflen);
void txSalusMessage(uint16_t pairingcode, uint8_t *txbuf1, uint8_t *txbuf2, uint8_t *txbuf3);
void txSalusPairing(uint16_t pairingcode);
void txSalusBoilerOn(uint16_t pairingcode);
void txSalusBoilerOff(uint16_t pairingcode);

#endif
