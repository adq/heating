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
 * 0x03      -- command byte (0x20: on, 0x00: off, 0x04: pairing)
 * 0x04/0x05 -- 16 bit fixed point controller temperature (divide value by 100).
 * 0x06/0x07 -- unknown (suspiciously has a value of 1400, which looks like a time for boiler scheduling, but doesn't reflect my configuration)
 * 0x08/0x09 -- unknown (sometimes has a value of 1900, which again looks suspicious)
 * 0x0a      -- unknown (usually has a value of 50)
 * 0x0b      -- checksum (simple sum of all other bytes mod 256)
 */

struct __attribute__((__packed__)) salus_v1_message {
    uint8_t pairing_code_msb;
    uint8_t command;
    uint8_t checksum;
    uint8_t padding[9];
};

struct __attribute__((__packed__)) salus_v2_message {
    uint8_t prefix;
    uint8_t pairingcode_msb;
    uint8_t pairingcode_lsb;
    uint8_t command;
    uint8_t padding[7];
    uint8_t checksum;
};

uint8_t salusChecksum(uint8_t * buf, int buflen) {
    uint8_t i = 0;
    uint8_t checksum = 0;
    while (i < buflen) {
        checksum += buf[i++];
    }

    return checksum;
}

void txSalusMessage(uint16_t pairingcode,
                    uint8_t salus_v1_command,
                    uint8_t salus_v2_command) {
    struct salus_v1_message msg1;
    struct salus_v2_message msg2;
    struct salus_v2_message msg3;

    memset(&msg1, 0x00, sizeof(struct salus_v1_message));
    msg1.command = salus_v1_command;
    msg1.pairing_code_msb = (uint8_t) (pairingcode >> 8);
    msg1.checksum = salusChecksum((uint8_t*) &msg1, 2);

    memset(&msg2, 0x00, sizeof(struct salus_v2_message));
    msg2.prefix = 0xa5;
    msg2.command = salus_v2_command;
    msg2.pairingcode_msb = (uint8_t) (pairingcode >> 8);
    msg2.pairingcode_msb = (uint8_t) pairingcode;
    msg2.checksum = salusChecksum((uint8_t*) &msg2, 11);

    memset(&msg2, 0x00, sizeof(struct salus_v2_message));
    msg2.prefix = 0xa7;
    msg2.command = salus_v2_command;
    msg2.pairingcode_msb = (uint8_t) (pairingcode >> 8);
    msg2.pairingcode_msb = (uint8_t) pairingcode;
    msg2.checksum = salusChecksum((uint8_t*) &msg2, 11);

    setTxMode();
    for(int i=0; i < 8; i++) {
        sendPacket((uint8_t*) &msg1, 12);
        usleep(30000);
        sendPacket((uint8_t*) &msg2, 12);
        usleep(30000);
        sendPacket((uint8_t*) &msg3, 12);
        usleep(30000);
    }
    setRxMode();
}

void txSalusPairing(uint16_t pairingcode) {
    // struct salus_v1_message msg1 = {command: 0xAA};
    // struct salus_v2_message msg2 = {command: 0x04};
    // struct salus_v2_message msg3 = {command: 0x04};

    // uint8_t txbuf1[] = {0x00, 0xAA, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    // uint8_t txbuf2[] = {0xa5, 0x00, 0x04, 0xAA, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};
    // uint8_t txbuf3[] = {0xa7, 0x00, 0x04, 0xAA, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};

    txSalusMessage(pairingcode, 0xAA, 0x04);
}

void txSalusBoilerOn(uint16_t pairingcode) {
    // struct salus_v1_message msg1 = {command: 0x01};
    // struct salus_v2_message msg2 = {command: 0x20};
    // struct salus_v2_message msg3 = {command: 0x20};

    // uint8_t txbuf1[] = {0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    // uint8_t txbuf2[] = {0xa5, 0x00, 0x00, 0x20, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};
    // uint8_t txbuf3[] = {0xa7, 0x00, 0x00, 0x20, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};

    txSalusMessage(pairingcode, 0x01, 0x20);
}

void txSalusBoilerOff(uint16_t pairingcode) {
    // struct salus_v1_message msg1 = {command: 0x02};
    // struct salus_v2_message msg2 = {command: 0x00};
    // struct salus_v2_message msg3 = {command: 0x00};

    // uint8_t txbuf1[] = {0x00, 0x02, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    // uint8_t txbuf2[] = {0xa5, 0x00, 0x00, 0x00, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};
    // uint8_t txbuf3[] = {0xa7, 0x00, 0x00, 0x00, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0x00};

    txSalusMessage(pairingcode, 0x02, 0x00);
}
