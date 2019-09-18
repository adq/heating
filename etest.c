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
    FILE *f;

    if (initHardware()) {
        fprintf(stderr, "Cannot initialize BCM2835\n.");
        exit(EXIT_FAILURE);
    }

    txOOKSwitch(0x12345, 0, 1);

    shutdownHardware();
}
