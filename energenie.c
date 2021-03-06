#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "energenie.h"
#include "radio.h"
#include "hw.h"


#define DEFAULT_TEMPERATURE 17

struct RadiatorSensor *sensors_root = NULL;
pthread_mutex_t sensors_root_mutex = PTHREAD_MUTEX_INITIALIZER;


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

uint8_t encode_bits(uint32_t data, uint8_t count, uint8_t *buf) {
    uint8_t shift = count - 2;
    uint8_t bits;

    for(int i=0; i< count / 2; i++) {
        switch ((data >> shift) & 0x03) {
        case 0:
            bits = 0x88;
            break;
        case 1:
            bits = 0x8E;
            break;
        case 2:
            bits = 0xE8;
            break;
        case 3:
            bits = 0xEE;
            break;
        }
        buf[i] = bits;
        shift -= 2;
    }

    return count / 2;
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
        sendPacket(txbuf, 5 + msglen + 3);
        usleep(30000);
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


void txExercise(uint32_t sensorid) {
    uint8_t txbuf[] = {sensorid >> 16,
                       sensorid >> 8,
                       sensorid,
                       OT_EXERCISE_VALVE,
                       0
                      };
    tx(txbuf, sizeof(txbuf));
}

void txOOKSwitch(uint32_t house_address, uint32_t device_address, uint8_t on_off) {
    uint8_t txbuf[20];
    uint8_t txpos = 0;
    uint8_t command_bits = 0;

    // message sync words used on subsequent retransmissions
    txbuf[0] = 0x80;
    txbuf[1] = 0x00;
    txbuf[2] = 0x00;
    txbuf[3] = 0x00;

    // the actual message itself
    txpos += 4;
    txpos += encode_bits(house_address, 20, txbuf + txpos);

    if (!on_off) {
        command_bits = 0x00;
    } else {
        command_bits = 0x01;
    }
    switch(device_address) {
    case 0:
        command_bits |= 0x0C;
        break;
    case 1:
        command_bits |= 0x0E;
        break;
    case 2:
        command_bits |= 0x06;
        break;
    case 3:
        command_bits |= 0x0A;
        break;
    case 4:
        command_bits |= 0x02;
        break;
    }
    txpos += encode_bits(command_bits, 4, txbuf + txpos);

    configEnergenieOOK();
    setTxMode();

    // send initial packet (sync bytes already sent by radio hardware)
    writeRegMultibyte(0, txbuf+4, 12);

    // send retries
    for(int i=0; i< 8; i++) {
        while(readReg(0x28) & 0x20) {
            usleep(1000);
        }
        writeRegMultibyte(0, txbuf, 16);
    }

    // wait for transmission to finish
    while(!(readReg(0x28) & 0x08)) {
        usleep(1000);
    }
    configOpenThingsFSK();
}


double decodeDouble(uint8_t *buf, int buflen) {
    double numvalue;
    if (decodeValue(&buf, buflen, &numvalue) != VT_NUMBER) {
        return 0;
    }
    return numvalue;
}


struct RadiatorSensor *energenie_loop() {
    uint8_t rxbuf[256];
    int pktlen, i;
    char topic[256];
    char strvalue[256];
    double numvalue;

    // wait for data
    if (!(readReg(0x28) & 0x04)) {
        usleep(50000);
        return NULL;
    }

    // extract packet data
    pktlen = readReg(0x00);
    if (pktlen >= sizeof(rxbuf)) {
        clearFIFO();
        return NULL;
    }
    for(i=0; i < pktlen; i++) {
        rxbuf[i] = readReg(0x00);
    }

    // check manufid/prodid
    if (pktlen < 2) {
        clearFIFO();
        return NULL;
    }
    uint8_t manufid = rxbuf[0];
    uint8_t prodid = rxbuf[1];
    if ((manufid != MANUFID_ENERGENIE) || (prodid != PRODID_ETRV)) {
        clearFIFO();
        return NULL;
    }

    // decrypt the packet data
    if (pktlen < 4) {
        clearFIFO();
        return NULL;
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
        return NULL;
    }

    // find the sensor
    if (pktlen < 7) {
        clearFIFO();
        return NULL;
    }
    uint32_t sensorid = (rxbuf[4] << 16) | (rxbuf[5] << 8) | rxbuf[6];

    // ok, find the sensor
    struct RadiatorSensor *sensor = find_sensor(sensorid);
    if (!sensor) {
        fprintf(stderr, "Saw unknown sensorid %i\n", sensorid);
        clearFIFO();
        return NULL;
    }

    // decode the message
    if (pktlen < 8) {
        clearFIFO();
        return NULL;
    }
    uint8_t paramid = rxbuf[7];
    switch(paramid) {
    case OT_TEMP_REPORT:
        sensor->temperature = decodeDouble(rxbuf+8, pktlen-8-3);
        break;

    case OT_VOLTAGE:
        sensor->voltage = decodeDouble(rxbuf+8, pktlen-8-3);
        break;

    default:
        fprintf(stderr -1, "Unknown message paramid %02x from sensorid %i\n", paramid, sensorid);
        break;
    }

    // send any transmissions if there are any. We only do this if we've just seen a message 'cos the device will
    // otherwise be sleeeeeeping!
    if (sensor->locate) {
        txIdentify(sensorid);
        sensor->locate = 0;

    } else if (sensor->exercise_valve) {
        txExercise(sensor->sensorid);
        sensor->exercise_valve = 0;

    } else if ((time(NULL) - sensor->desiredTemperatureTxStamp) > DESIREDTEMP_SECS) {
        uint8_t desiredTemperature = sensor->desiredTemperature;
        if (0 == desiredTemperature) {
            desiredTemperature = DEFAULT_TEMPERATURE;
        }
        txDesiredTemperature(sensor->sensorid, desiredTemperature);
        sensor->desiredTemperatureTxStamp = time(NULL);

    } else if ((time(NULL) - sensor->voltageRxStamp) > ASKVOLTAGE_SECS) {
        txRequestVoltage(sensor->sensorid);
        sensor->voltageRxStamp = time(NULL);
    }

    // cleanup and return sensor
    sensor->lastRxStamp = time(NULL);
    clearFIFO();
    return sensor;
}


struct RadiatorSensor *find_sensor(uint32_t sensorid) {
    pthread_mutex_lock(&sensors_root_mutex);

    // try and find sensor in list
    struct RadiatorSensor *cur = sensors_root;
    while(cur) {
        if (cur->sensorid == sensorid) {
            pthread_mutex_unlock(&sensors_root_mutex);
            return cur;
        }
        cur = cur->next;
    }

    // ok, try and allocate space for a new one
    if (NULL == (cur = malloc(sizeof(struct RadiatorSensor)))) {
        pthread_mutex_unlock(&sensors_root_mutex);
        return NULL;
    }

    // setup the initial structure
    memset(cur, 0, sizeof(struct RadiatorSensor));
    cur->sensorid = sensorid;
    cur->next = sensors_root;
    sensors_root = cur;

    // return the newly created sensor record
    pthread_mutex_unlock(&sensors_root_mutex);
    return cur;
}
