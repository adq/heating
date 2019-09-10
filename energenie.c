#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mosquitto.h>
#include "energenie.h"
#include "radio.h"
#include "hw.h"


struct RadiatorSensor *sensors_root = NULL;

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


void txRequestVoltage(uint32_t sensorid) {
    uint8_t txbuf[] = {sensorid >> 16,
                       sensorid >> 8,
                       sensorid,
                       OT_REQUEST_VOLTAGE,
                       0
                      };
    tx(txbuf, sizeof(txbuf));
}


void txDesiredTemperature(uint32_t sensorid, uint8_t desiredTemperature) {
    uint8_t txbuf[] = {sensorid >> 16,
                       sensorid >> 8,
                       sensorid,
                       OT_TEMP_SET,
                       0x92,
                       desiredTemperature,
                       0
                      };
    tx(txbuf, sizeof(txbuf));
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


double decodeTemperature(uint8_t *buf, int buflen) {
    double numvalue;
    if (decodeValue(&buf, buflen, &numvalue) != VT_NUMBER) {
        return -1;
    }
    return numvalue;
}


double decodeVoltage(uint8_t *buf, int buflen) {
    double numvalue;
    if (decodeValue(&buf, buflen, &numvalue) != VT_NUMBER) {
        return -1;
    }
    return numvalue;
}


void publishTemperature(struct mosquitto *mosq, uint32_t sensorid, double d) {
    char topic[256];
    char strvalue[256];

    sprintf(strvalue, "%f", d);
    sprintf(topic, "/radiator/%i/temperature", sensorid);
    mosquitto_publish(mosq, NULL, topic, strlen(strvalue), strvalue, 0, true);
}


void publishLocate(struct mosquitto *mosq, uint32_t sensorid, int i) {
    char topic[256];
    char strvalue[256];

    sprintf(strvalue, "%i", i);
    sprintf(topic, "/radiator/%i/locate", sensorid);
    mosquitto_publish(mosq, NULL, topic, strlen(strvalue), strvalue, 0, true);
}


void publishRXStamp(struct mosquitto *mosq, uint32_t sensorid) {
    char topic[256];
    char strvalue[256];

    sprintf(strvalue, "%i", time(NULL));
    sprintf(topic, "/radiator/%i/last_rx_stamp", sensorid);
    mosquitto_publish(mosq, NULL, topic, strlen(strvalue), strvalue, 0, true);
}


void energenie_loop(struct mosquitto *mosq) {
    uint8_t rxbuf[256];
    int pktlen, i;
    char topic[256];
    char strvalue[256];
    double numvalue;

    // wait for data
    if (!(readReg(0x28) & 0x04)) {
        usleep(10000);
        return;
    }

    // extract packet data
    pktlen = readReg(0x00);
    assert(pktlen < sizeof(rxbuf));
    for(i=0; i < pktlen; i++) {
        rxbuf[i] = readReg(0x00);
    }

    // check manufid/prodid
    if (pktlen < 2) {
        clearFIFO();
        return;
    }
    uint8_t manufid = rxbuf[0];
    uint8_t prodid = rxbuf[1];
    if ((manufid != MANUFID_ENERGENIE) || (prodid != PRODID_ETRV)) {
        clearFIFO();
        return;
    }

    // decrypt the packet data
    if (pktlen < 4) {
        clearFIFO();
        return;
    }
    uint16_t ran;
    uint16_t pip = (rxbuf[2] << 8) | rxbuf[3];
    seedcrypt(&ran, PID_ENERGENIE, pip);
    for(i=4; i < pktlen; i++) {
        rxbuf[i] = cryptbyte(&ran, rxbuf[i]);
    }

    // check the CRC (final two bytes contains crc value, so should compute to 0 if correct)
    if (crc(rxbuf+4, pktlen-4)) {
        clearFIFO();
        return;
    }

    // find the sensor
    if (pktlen < 7) {
        clearFIFO();
        return;
    }
    uint32_t sensorid = (rxbuf[4] << 16) | (rxbuf[5] << 8) | rxbuf[6];

    // ok, find the sensor
    struct RadiatorSensor *sensor = findSensor(sensorid);
    if (!sensor) {
        fprintf(stderr, "Saw unknown sensorid %i\n", sensorid);
        clearFIFO();
        return;
    }

    // decode the message
    if (pktlen < 8) {
        clearFIFO();
        return;
    }
    uint8_t paramid = rxbuf[7];
    switch(paramid) {
    case OT_TEMP_REPORT:
        double temp = decodeTemperature(rxbuf+8, pktlen-8-3);
        publishDouble(mosq, sensorid, temp);
        publishRXStamp(mosq, sensorid);
        break;

    case OT_VOLTAGE:
        double voltage = decodeVoltage(rxbuf+8, pktlen-8-3);
        publishDouble(mosq, sensorid, voltage);
        publishRXStamp(mosq, sensorid);
        break;

    default:
        fprintf(stderr -1, "Unknown message paramid %02x from sensorid %i\n", paramid, sensorid);
        break;
    }

    // send locate message if we've been asked to
    if (sensor->locate) {
        txIdentify(sensorid);
        publishLocate(mosq, sensorid, 0);
        sensor->locate = false;
    }

    // send any transmissions if there are any. We only do this if we've just seen a message 'cos the device will
    // otherwise be sleeeeeeping!
    if ((time(NULL) - sensor->temperatureTxStamp) > DESIREDTEMP_SECS) {
        txDesiredTemperature(sensor);
    } else if ((time(NULL) - sensor->voltageRxStamp) > ASKVOLTAGE_SECS) {
        txRequestVoltage(sensor);
    }

    clearFIFO();
}


struct RadiatorSensor *findSensor(uint32_t sensorid) {
    struct RadiatorSensor *cur = sensors_root;

    // try and fina sensor in list
    while(cur) {
        if (cur->sensorid == sensorid) {
            return cur;
        }
        cur = cur->next;
    }

    // ok, try and allocate space for a new one
    if (NULL == (cur = malloc(sizeof(struct RadiatorSensor)))) {
        return NULL;
    }

    // setup the initial structure
    cur = memset(cur, 0, sizeof(struct RadiatorSensor));
    cur->sensorid = sensorid;
    cur->next = sensors_root;
    sensors_root = cur;

    // return the newly created sensor record
    return cur;
}
