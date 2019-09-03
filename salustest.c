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


int main(int argc, char *argv[]) {
    int i;

    if (initHardware()) {
        fprintf(stderr, "Cannot initialize BCM2835\n.");
        exit(EXIT_FAILURE);
    }

    // init the 868 Mhz module
    bcm2835_spi_chipSelect(CS_868MHZ);

    // test turning it on
    uint8_t txbuf[] = {0x45, 0x01, 0x46, 0x55}; // on
    // uint8_t txbuf[] = {0x45, 0x02, 0x47, 0x55}; // off

    while(1) {
        setTxMode();
        for(i=0; i < 10; i++) {
            writeRegMultibyte(0, txbuf, sizeof(txbuf));

            // check FIFO level
            while(readReg(0x28) & 0x20) {
                usleep(1000);
            }
            while(!(readReg(0x28) & 0x08)) {
                usleep(1000);
            }
            usleep(100000);
        }
        setRxMode();
        printf("OK\n");

        printf("PRESS A KEY\n");
        getchar();
    }

    shutdownHardware();
}
