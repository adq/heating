#include <bcm2835.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "hw.h"
#include "radio.h"


void resetRFModules() {
    bcm2835_gpio_fsel(RESET_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(RESET_PIN, HIGH);
    usleep(10000);
    bcm2835_gpio_write(RESET_PIN, LOW);
    usleep(10000);
}


void spiMode() {
    bcm2835_spi_begin();
    bcm2835_spi_setClockDivider(26);  // Clock Divider value 250MHz / 26 = 9.6MHz
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
}


void writeReg(uint8_t addr, uint8_t value) {
    uint8_t buf[2];
    buf[0] = addr | 0x80;
    buf[1] = value;
    bcm2835_spi_writenb(buf, 2);
}


void writeRegMultibyte(uint8_t addr, uint8_t *values, uint8_t len) {
    uint8_t buf[256];
    buf[0] = addr | 0x80;
    memcpy(buf+1, values, len);
    bcm2835_spi_writenb(buf, len + 1);
}


uint8_t readReg(uint8_t addr) {
    uint8_t buf[2];
    buf[0] = addr;
    buf[1] = 0x00;

    bcm2835_spi_transfern(buf, 2);
    return buf[1];
}


int initHardware() {
    // set everything up!
    if (!bcm2835_init()) {
        return 1;
    }
    resetRFModules();
    spiMode();

    // init the 868 Mhz module
    bcm2835_spi_chipSelect(CS_868MHZ);
    configSalusFSK();
    clearFIFO();

    // init the 433 Mhz module
    bcm2835_spi_chipSelect(CS_433MHZ);
    configOpenThingsFSK();
    clearFIFO();

    return 0;
}


void shutdownHardware() {
    resetRFModules();
    bcm2835_spi_end();
}
