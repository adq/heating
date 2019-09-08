#ifndef RADIO_H
#define RADIO_H 1

#include <stdint.h>

void clearFIFO();
void setTxMode();
void setRxMode();
void sendPacket(uint8_t *txbuf, int txbuflen);
void configOpenThingsFSK();
void configSalusFSK();

#endif
