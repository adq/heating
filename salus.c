#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "radio.h"
#include "hw.h"
#include "salus.h"


/**
 * Protocol: There are three packets sent.
 *
 * Packet 0 appears to be a legacy compatability packet. It is three bytes long. It has only a single byte for the pairing code.
 * 0x00 -- MSB of 16 bit boiler pairing code
 * 0x01 -- command byte (0x01: on, 0x02: off, 0xaa: pairing)
 * 0x02 -- checksum (simple sum of all other bytes mod 256)
 *
 * Packets 1 and packet 2 are identical 12 bytes packets, but they start with 0xa5, or 0xa7 for unknown reasons.
 * Both are trasmitted one after another, but it appears either one will work on my boiler.
 * The pairing code has been increased to 16 bytes with this next version of the protocol.
 *
 * 0x00      -- unknown -- 0xa5 or 0xa7
 * 0x01      -- MSB of 16 byte boiler pairing code
 * 0x02      -- LSB of 16 byte boiler pairing code
 * 0x03      -- command byte (0x20: on, 0x00: off, 0x??: pairing)
 * 0x04/0x05 -- 16 bit fixed point controller temperature (divide value by 100).
 * 0x06/0x07 -- unknown (suspiciously has a value of 1400, which looks like a time for boiler scheduling, but doesn't reflect my configuration)
 * 0x08/0x09 -- unknown (sometimes has a value of 1900, which again looks suspicious)
 * 0x0a      -- unknown (usually has a value of 50)
 * 0x0b      -- checksum (simple sum of all other bytes mod 256)
 */



uint8_t salusChecksum(uint8_t * buf, int buflen) {
    uint8_t i = 0;
    uint8_t checksum = 0;
    while (i < buflen) {
        checksum += buf[i++];
    }

    return checksum;
}

void txSalusMessage(uint16_t pairingcode, uint8_t *txbuf1, uint8_t *txbuf2, uint8_t *txbuf3) {
    txbuf1[0] = (uint8_t) (pairingcode >> 8);
    txbuf1[2] = salusChecksum(txbuf1, 2);

    txbuf2[1] = (uint8_t) (pairingcode >> 8);
    txbuf2[2] = (uint8_t) pairingcode;
    txbuf2[11] = salusChecksum(txbuf2, 11);

    txbuf3[1] = (uint8_t) (pairingcode >> 8);
    txbuf3[2] = (uint8_t) pairingcode;
    txbuf3[11] = salusChecksum(txbuf3, 11);

    setTxMode();
    for(int i=0; i < 8; i++) {
        sendPacket(txbuf1, 12);
        usleep(30000);
        sendPacket(txbuf2, 12);
        usleep(30000);
        sendPacket(txbuf3, 12);
        usleep(30000);
    }
    setRxMode();
}

void txSalusPairing(uint16_t pairingcode) {
    uint8_t txbuf1[] = {0x00, 0xAA, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t txbuf2[] = {0xa5, 0x00, 0x00, 0xAA, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};
    uint8_t txbuf3[] = {0xa7, 0x00, 0x00, 0xAA, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};

    txSalusMessage(pairingcode, txbuf1, txbuf2, txbuf3);
}

void txSalusBoilerOn(uint16_t pairingcode) {
    uint8_t txbuf1[] = {0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t txbuf2[] = {0xa5, 0x00, 0x00, 0x20, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};
    uint8_t txbuf3[] = {0xa7, 0x00, 0x00, 0x20, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};

    txSalusMessage(pairingcode, txbuf1, txbuf2, txbuf3);
}

void txSalusBoilerOff(uint16_t pairingcode) {
    uint8_t txbuf1[] = {0x00, 0x02, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8_t txbuf2[] = {0xa5, 0x00, 0x00, 0x00, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};
    uint8_t txbuf3[] = {0xa7, 0x00, 0x00, 0x00, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};

    txSalusMessage(pairingcode, txbuf1, txbuf2, txbuf3);
}
