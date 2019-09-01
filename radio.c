#include <unistd.h>
#include "radio.h"
#include "hw.h"


void clearFIFO() {
    while(readReg(0x28) & 0x40) {
        readReg(0x00);
    }
}


void setTxMode() {
    writeReg(0x01, 0x0c);
    while((readReg(0x27) & 0xA0) != 0xA0) {
        usleep(1000);
    }
}


void setRxMode() {
    writeReg(0x01, 0x10);
    while((readReg(0x27) & 0x80) != 0x80) {
        usleep(1000);
    }
}


void configOpenThingsFSK() {
    // FSK modulation scheme
    writeReg(0x02, 0x00);

    // frequency deviation 30kHz (0x1ec)
    writeReg(0x05, 0x01);
    writeReg(0x06, 0xEC);

    // carrier frequency 434.3Mhz (0x6c9333)
    writeReg(0x07, 0x6c);
    writeReg(0x08, 0x93);
    writeReg(0x09, 0x33);

    // standard AFC
    writeReg(0x0B, 0x00);

    // 50Ohms
    writeReg(0x18, 0x08);

    // channel filter bandwidth 0xkHz
    writeReg(0x19, 0x43);

    // preamble LSB 3 bytes
    writeReg(0x2d, 0x03);

    // sync config
    writeReg(0x2e, 0x88);
    writeReg(0x2f, 0x2d);
    writeReg(0x30, 0xd4);

    // packet config (variable length, manchester coded, address must match node address)
    writeReg(0x37, 0xa2);

    // max payload length
    writeReg(0x38, 0x40);

    // node address
    writeReg(0x39, 0x04);

    // FIFO TX threshold : at least 1 byte
    writeReg(0x3C, 0x81);

    setRxMode();
}


void configSalusFSK() {
    // FSK modulation scheme
    writeReg(0x02, 0x00);

    // bit rate 2.4kbit hex(round(32000000 / 2400))
    writeReg(0x03, 0x34);
    writeReg(0x04, 0x15);

    // frequency deviation 125kHz (round(62500 / (32000000 / math.pow(2,19))))
    writeReg(0x05, 0x04);
    writeReg(0x06, 0x00);

    // carrier frequency 868.260Mhz (round(868260000 / (32000000 / math.pow(2,19))))
    writeReg(0x07, 0xd9);
    writeReg(0x08, 0x10);
    writeReg(0x09, 0xa4);

    // standard AFC
    writeReg(0x0B, 0x00);

    // 50Ohms
    writeReg(0x18, 0x08);

    // channel filter bandwidth 2.6kHz
    // writeReg(0x19, 0x57);   // FIXME

    // preamble LSB 3 bytes
    writeReg(0x2d, 0x03);

    // sync config
    writeReg(0x2e, 0x88);
    writeReg(0x2f, 0x2d);
    writeReg(0x30, 0xd4);

    // packet config (variable length)
    writeReg(0x37, 0x80);

    // max payload length
    writeReg(0x38, 0x40);

    // FIFO TX threshold : at least 1 byte
    writeReg(0x3C, 0x81);

    setRxMode();
}
