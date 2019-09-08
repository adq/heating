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
    setRxMode();

    while(1) {
        // wait for data
        if (!(readReg(0x28) & 0x04)) {
            usleep(10000);
            continue;
        }

        fprintf(f, "%20i ", time(NULL));
        for(int i=0; i < 12; i++) {
            fprintf(f, "%02x ", readReg(0x00));
        }
        fprintf(f, "\n");
        fflush(f);

        clearFIFO();
    }

    shutdownHardware();
}
