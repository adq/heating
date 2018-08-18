#include <bcm2835.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <assert.h>
#include <pthread.h>


#define RESET_PIN RPI_V2_GPIO_P1_22
#define CS_433MHZ BCM2835_SPI_CS1
#define CS_868MHZ BCM2835_SPI_CS0

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

struct RadiatorSensor {
    char *name;
    uint32_t sensorid;

    double desiredTemperature;
    double temperature;
    time_t temperatureRxStamp;
    time_t temperatureTxStamp;

    double voltage;
    time_t voltageRxStamp;
};

struct RadiatorSensor sensors[] = {
    {"frontroom", 2441, 17, 0, 0, 0, 0, 0},
    {"kitchen",   3702, 17, 0, 0, 0, 0, 0},
    {"hall",      4173, 17, 0, 0, 0, 0, 0},
    {"bedroom",   3809, 17, 0, 0, 0, 0, 0},
};


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


void configFSK() {
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


struct RadiatorSensor *findSensor(uint32_t sensorid) {
    for(int i=0; i < sizeof(sensors) / sizeof(struct RadiatorSensor); i++) {
        if (sensors[i].sensorid == sensorid) {
            return &sensors[i];
        }
    }
    return NULL;
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


void *writeCsv(void*arg) {
    char tbuf[20];
    char filename[256];
    struct tm tresult;

    while(1) {
        sleep(60);

        // format filename correctly for current day
        time_t now = time(NULL);
        strftime(tbuf, 20, "%Y-%m-%d", localtime_r(&now, &tresult));
        sprintf(filename, "/tmp/%s.csv", tbuf);
        FILE *f = fopen(filename, "a");
        if (!f) {
            continue;
        }

        // write out a line per sensor
        for(int i=0; i<sizeof(sensors) / sizeof(struct RadiatorSensor); i++) {
            fprintf(f, "%s,%i,%g,%g,%i,%i,%g,%i\n",
                    sensors[i].name,
                    sensors[i].sensorid,
                    sensors[i].desiredTemperature,
                    sensors[i].temperature,
                    sensors[i].temperatureTxStamp,
                    sensors[i].temperatureRxStamp,
                    sensors[i].voltage,
                    sensors[i].voltageRxStamp);
        }

        fflush(f);
        fclose(f);
    }
}


int main(int argc, char *argv[]) {
    uint8_t rxbuf[256];
    int pktlen, i;
    char strvalue[256];
    double numvalue;
    uint32_t identifySensorId = 0;
    int opt;
    pthread_t csvthread;


    // parse args
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        switch (opt) {
        case 'i':
            identifySensorId = atoi(optarg);
            fprintf(stderr, "Waiting for sensor %i to appear...\n", identifySensorId);
            break;

        default:
            fprintf(stderr, "Usage: %s [-i sensorid to identify]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // set everything up!
    if (!bcm2835_init()) {
        fprintf(stderr, "Cannot initialize BCM2835\n.");
        exit(EXIT_FAILURE);
    }
    resetRFModules();
    spiMode();

    // start up the csv writer!
    pthread_create(&csvthread, NULL, writeCsv, NULL);

    // init the 433 Mhz module
    bcm2835_spi_chipSelect(CS_433MHZ);
    configFSK();
    clearFIFO();

    while(1) {
        // wait for data
        if (!(readReg(0x28) & 0x04)) {
            usleep(10000);
            continue;
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
            continue;
        }
        uint8_t manufid = rxbuf[0];
        uint8_t prodid = rxbuf[1];
        if ((manufid != MANUFID_ENERGENIE) || (prodid != PRODID_ETRV)) {
            clearFIFO();
            continue;
        }

        // decrypt the packet data
        if (pktlen < 4) {
            clearFIFO();
            continue;
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
            continue;
        }

        // find the sensor
        if (pktlen < 7) {
            clearFIFO();
            continue;
        }
        uint32_t sensorid = (rxbuf[4] << 16) | (rxbuf[5] << 8) | rxbuf[6];

        // deal with identification if we've been asked to
        if (identifySensorId) {
            if (sensorid == identifySensorId) {
                fprintf(stderr, ":) Requesting sensor %i to identify itself\n", sensorid);
                txIdentify(sensorid);
            } else {
                fprintf(stderr, ":( Ignoring sensor %i in identify mode\n", sensorid);
            }
            clearFIFO();
            continue;
        }

        // ok, find the sensor
        struct RadiatorSensor *sensor = findSensor(sensorid);
        if (!sensor) {
            fprintf(stderr, "Saw unknown sensorid %i\n", sensorid);
            clearFIFO();
            continue;
        }

        // decode the message
        if (pktlen < 8) {
            clearFIFO();
            continue;
        }
        uint8_t paramid = rxbuf[7];
        switch(paramid) {
        case OT_TEMP_REPORT:
            rxTemperature(sensor, rxbuf+8, pktlen-8-3);
            break;

        case OT_VOLTAGE:
            rxVoltage(sensor, rxbuf+8, pktlen-8-3);
            break;

        default:
            fprintf(stderr, "Unknown message paramid %02x from sensorid %i\n", paramid, sensorid);
            break;
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

    bcm2835_spi_end();
}
