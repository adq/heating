#ifndef ENERGENIE_H
#define ENERGENIE_H 1

#include <stdint.h>
#include <time.h>

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

void seedcrypt(uint16_t *ran, uint8_t pid, uint16_t pip);
uint8_t cryptbyte(uint16_t *ran, uint8_t dat);
int16_t crc(uint8_t const mes[], unsigned char siz);
int decodeValue(uint8_t** buf, int buflen, double *outNumber);
void tx(uint8_t *msg, uint8_t msglen);
void txRequestVoltage(struct RadiatorSensor *sensor);
void txDesiredTemperature(struct RadiatorSensor *sensor);
void txIdentify(uint32_t sensorid);
void rxTemperature(struct RadiatorSensor *sensor, uint8_t *buf, int buflen);
void rxVoltage(struct RadiatorSensor *sensor, uint8_t *buf, int buflen);

#endif
