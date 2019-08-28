#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "energenie.h"
#include "radio.h"
#include "hw.h"


void seedcrypt(uint16_t *ran, uint8_t pid, uint16_t pip) {
    *ran = (((uint16_t) pid) << 8) ^ pip;
}


uint8_t cryptbyte(uint16_t *ran, uint8_t dat) {
    int i;
    for(i=0; i < 5; i++) {
        *ran = (*ran & 1) ? ((*ran >> 1) ^ 0xf5f5U) : (*ran >> 1);
    }
    return (uint8_t) (*ran ^ dat ^ 0x5aU);
}


int16_t crc(uint8_t const mes[], unsigned char siz)
{
    uint16_t rem = 0;
    unsigned char byte, bit;

    for (byte = 0; byte < siz; ++byte) {
        rem ^= (mes[byte] << 8);
        for (bit = 8; bit > 0; --bit) {
            rem = ((rem & (1 << 15)) ? ((rem << 1) ^ 0x1021) : (rem << 1));
        }
    }
    return rem;
}


int decodeValue(uint8_t** buf, int buflen, double *outNumber) {
    // do we have space?
    if (buflen < 1) {
        return -1;
    }

    uint8_t datatype = **buf >> 4;
    uint8_t datalen = **buf & 0x0f;
    (*buf)++;

    // do we have space?
    if (buflen < (1 + datalen)) {
        return -1;
    }

    // string values
    if (datatype == 7) {
        fprintf(stderr, "Unsupported datatype %i\n", datatype);
        return -1;
    }

    // extract numerical data
    uint64_t tmp = 0;
    uint64_t mask = 0;
    while(datalen--) {
        tmp = (tmp << 8) | **buf;
        mask = (mask << 8) | 0xff;
        (*buf)++;
    }

    // unsigned integers
    if (datatype < 7) {
        *outNumber = (double) tmp / (1 << (4*datatype));
        return VT_NUMBER;
    }

    // signed integers
    if ((datatype >= 8) && (datatype < 12)) {
        if (*buf[0] & 0x80) {
            tmp = (~tmp) & mask;
            tmp = -(tmp + 1);
        }
        *outNumber = (double) tmp / (1 << (8*(datatype-8)));
        return VT_NUMBER;
    }

    fprintf(stderr, "Unsupported datatype %i\n", datatype);
    return -1;
}


void tx(uint8_t *msg, uint8_t msglen) {
    uint16_t pip = rand();
    uint16_t ran;
    int i;

    uint8_t txbuf[256];
    txbuf[0] = 5 + msglen + 3 - 1;  // -1 as we do not include the length field itself
    txbuf[1] = MANUFID_ENERGENIE;
    txbuf[2] = PRODID_ETRV;
    txbuf[3] = pip >> 8;
    txbuf[4] = pip;
    memcpy(txbuf + 5, msg, msglen);
    txbuf[5 + msglen] = 0;
    txbuf[5 + msglen + 1] = 0;
    txbuf[5 + msglen + 2] = 0;

    // compute and store CRC
    uint16_t bufcrc = crc(txbuf + 5, msglen + 1);
    txbuf[5 + msglen + 1] = bufcrc >> 8;
    txbuf[5 + msglen + 2] = bufcrc;

    // encrypt the message
    seedcrypt(&ran, PID_ENERGENIE, pip);
    for(i=0; i < msglen + 3; i++) {
        txbuf[5 + i] = cryptbyte(&ran, txbuf[5 + i]);
    }

    // send it
    setTxMode();

    // send it (multiple times)
    for(i=0; i < MESSAGE_RETRIES; i++) {
        writeRegMultibyte(0, txbuf, 5 + msglen + 3);

        // check FIFO level
        while(readReg(0x28) & 0x20) {
            usleep(1000);
        }
    }

    // wait for FIFO to empty itself
    while(readReg(0x28) & 0x40) {
        usleep(1000);
    }

    // aaand back to rx mode again
    setRxMode();
}


void txRequestVoltage(struct RadiatorSensor *sensor) {
    uint8_t txbuf[] = {sensor->sensorid >> 16,
                       sensor->sensorid >> 8,
                       sensor->sensorid,
                       OT_REQUEST_VOLTAGE,
                       0
                      };
    tx(txbuf, sizeof(txbuf));
}


void txDesiredTemperature(struct RadiatorSensor *sensor) {
    uint8_t txbuf[] = {sensor->sensorid >> 16,
                       sensor->sensorid >> 8,
                       sensor->sensorid,
                       OT_TEMP_SET,
                       0x92,
                       sensor->desiredTemperature,
                       0
                      };
    tx(txbuf, sizeof(txbuf));
    sensor->temperatureTxStamp = time(NULL);
}


void txIdentify(uint32_t sensorid) {
    uint8_t txbuf[] = {sensorid >> 16,
                       sensorid >> 8,
                       sensorid,
                       OT_IDENTIFY,
                       0
                      };
    tx(txbuf, sizeof(txbuf));
}


void rxTemperature(struct RadiatorSensor *sensor, uint8_t *buf, int buflen) {
    double numvalue;
    if (decodeValue(&buf, buflen, &numvalue) != VT_NUMBER) {
        return;
    }

    printf("SENSOR %s(%i) temp %g\n", sensor->name, sensor->sensorid, numvalue);

    sensor->temperatureRxStamp = time(NULL);
    sensor->temperature = numvalue;
}


void rxVoltage(struct RadiatorSensor *sensor, uint8_t *buf, int buflen) {
    double numvalue;
    if (decodeValue(&buf, buflen, &numvalue) != VT_NUMBER) {
        return;
    }

    printf("SENSOR %s(%i) voltage %g\n", sensor->name, sensor->sensorid, numvalue);

    sensor->voltageRxStamp = time(NULL);
    sensor->voltage = numvalue;
}
