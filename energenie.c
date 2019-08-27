#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "energenie.h"
#include "radio.h"
#include "lowlevel.h"


#define VT_STRING 0
#define VT_NUMBER 1


#define MANUFID_ENERGENIE 4
#define PRODID_ETRV 3

#define PID_ENERGENIE 0xf2

#define MESSAGE_RETRIES 8

#define ASKVOLTAGE_SECS (60*60)
#define DESIREDTEMP_SECS (60*60)

#define OT_JOIN_RESP	0x6A
#define OT_JOIN_CMD		0xEA

#define OT_POWER		0x70
#define OT_REACTIVE_P	0x71

#define OT_CURRENT		0x69
#define OT_ACTUATE_SW	0xF3
#define OT_FREQUENCY	0x66
#define OT_TEST			0xAA
#define OT_SW_STATE		0x73

#define OT_TEMP_SET		0xf4    /* Send new target temperature to driver board */
#define OT_TEMP_REPORT	0x74    /* Send externally read room temperature to motor board */


#define OT_VOLTAGE		0x76

#define OT_EXERCISE_VALVE   0xA3    /* Send exercise valve command to driver board.
                                       Read diagnostic flags returned by driver board.
                                       Send diagnostic flag acknowledgement to driver board.
                                       Report diagnostic flags to the gateway.
                                       Flash red LED once every 5 seconds if âbattery deadâ flag
                                       is set.
                                         Unsigned Integer Length 0
                                    */

#define OT_REQUEST_VOLTAGE 0xE2     /* Request battery voltage from driver board.
                                       Report battery voltage to gateway.
                                       Flash red LED 2 times every 5 seconds if voltage
                                       is less than 2.4V
                                         Unsigned Integer Length 0
                                       */
#define OT_REPORT_VOLTAGE   0x62    /* Volts
                                         Unsigned Integer Length 0
                                         */

#define OT_REQUEST_DIAGNOTICS 0xA6  /*   Read diagnostic flags from driver board and report
                                         these to gateway Flash red LED once every 5 seconds
                                         if âbattery deadâ flag is set
                                         Unsigned Integer Length 0
                                         */

#define OT_REPORT_DIAGNOSTICS 0x26

#define OT_SET_VALVE_STATE 0xA5     /*
                                       Send a message to the driver board
                                       0 = Set Valve Fully Open
                                       1=Set Valve Fully Closed
                                       2 = Set Normal Operation
                                       Valve remains either fully open or fully closed until
                                       valve state is set to ânormal operationâ.
                                       Red LED flashes continuously while motor is running
                                       terminated by three long green LED flashes when valve
                                       fully open or three long red LED flashes when valve is
                                       closed

                                       Unsigned Integer Length 1
                                       */

#define OT_SET_LOW_POWER_MODE 0xA4  /*
                                       0=Low power mode off
                                       1=Low power mode on

                                       Unsigned Integer Length 1
                                       */
#define OT_IDENTIFY     0xBF

#define OT_SET_REPORTING_INTERVAL 0xD2 /*
                                          Update reporting interval to requested value

                                       Unsigned Integer Length 2
                                          */


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
