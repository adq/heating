#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include "energenie.h"
#include "lowlevel.h"


struct RadiatorSensor sensors[] = {
    {"frontroom", 2441, 17, 0, 0, 0, 0, 0},
    {"kitchen",   3702, 17, 0, 0, 0, 0, 0},
    {"hall",      4173, 17, 0, 0, 0, 0, 0},
    {"bedroom",   3809, 17, 0, 0, 0, 0, 0},
};


struct RadiatorSensor *findSensor(uint32_t sensorid) {
    for(int i=0; i < sizeof(sensors) / sizeof(struct RadiatorSensor); i++) {
        if (sensors[i].sensorid == sensorid) {
            return &sensors[i];
        }
    }
    return NULL;
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

    // init the 868 Mhz module
    bcm2835_spi_chipSelect(CS_868MHZ);
    configSalusFSK();
    clearFIFO();


    // test turning it on
    uint8_t txbuf[] = {0x18, 0x01, 0x19, 0x5a};
    setTxMode();
    for(i=0; i < 30; i++) {
        writeRegMultibyte(0, txbuf, sizeof(txbuf));

        // check FIFO level
        while(readReg(0x28) & 0x20) {
            usleep(1000);
        }
        usleep(30000);
    }
    while(readReg(0x28) & 0x40) {
        usleep(1000);
    }
    setRxMode();



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
            printf("%02x ", readReg(0x00));
        }
        printf("\n");

        clearFIFO();
    }


    // start up the csv writer!
    pthread_create(&csvthread, NULL, writeCsv, NULL);

    // init the 433 Mhz module
    bcm2835_spi_chipSelect(CS_433MHZ);
    configOpenThingsFSK();
    clearFIFO();

    // main loop
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
