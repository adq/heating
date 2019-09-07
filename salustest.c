#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <bcm2835.h>
#include "energenie.h"
#include "hw.h"
#include "radio.h"


void sendpacket(uint8_t *txbuf, int txbuflen) {
    writeRegMultibyte(0, txbuf, txbuflen);

    // check FIFO level
    while(readReg(0x28) & 0x20) {
        usleep(1000);
    }
    while(!(readReg(0x28) & 0x08)) {
        usleep(1000);
    }
}

int main(int argc, char *argv[]) {
    int i;

    if (initHardware()) {
        fprintf(stderr, "Cannot initialize BCM2835\n.");
        exit(EXIT_FAILURE);
    }

    // init the 868 Mhz module
    bcm2835_spi_chipSelect(CS_868MHZ);

    // test turning it on
    uint8_t txbuf1[] = {0x47, 0x01, 0x4F, 0x55, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // on
    // uint8_t txbuf1[] = {0x45, 0x01, 0x46, 0x55, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; // on
    // uint8_t txbuf2[] = {0xa5, 0x45, 0x0e, 0x20, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0xd4};
    // uint8_t txbuf3[] = {0xa7, 0x45, 0x0e, 0x20, 0xd3, 0x09, 0xe8, 0x03, 0xb8, 0x0b, 0x32, 0xd6};

    while(1) {
        setTxMode();
        for(i=0; i < 8; i++) {
            sendpacket(txbuf1, sizeof(txbuf1));
            usleep(30000);
            // sendpacket(txbuf2, sizeof(txbuf2));
            // usleep(30000);
            // sendpacket(txbuf3, sizeof(txbuf3));
            // usleep(30000);
        }
        setRxMode();

        printf("OK\n");
        printf("PRESS A KEY\n");
        getchar();
    }

    shutdownHardware();
}
