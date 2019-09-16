#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <bcm2835.h>
#include <mosquitto.h>
#include <string.h>
#include <regex.h>
#include "energenie.h"
#include "salus.h"
#include "hw.h"
#include "radio.h"


regex_t topic_regex;
uint16_t boiler_pairing_code = 0;
bool boiler_pair_now = false;
bool boiler_state = false;
bool boiler_desired_state = false;
time_t boiler_tx_stamp = 0;

#define BOILER_TX_SECS (3*60)


char *get_regex_match_string(char *source, regmatch_t* match, char *buf) {
    int len;

    if (match->rm_so < 0) {
        buf[0] = 0;
        return buf;
    }

    len = match->rm_eo - match->rm_so;
    strncpy(buf, source + match->rm_so, len);
    buf[len] = 0;

    return buf;
}


void heating_mosquitto_message_radiator(const struct mosquitto_message * msg) {
    int err;
    regmatch_t matches[3];
    char subtopic[256];
    char value[256];
    uint16_t sensorid;

    if (err = regexec(&topic_regex, msg->topic, 3, matches, 0)) {
        fprintf(stderr, "Failed to match topic %s\n", msg->topic);
        return;
    }

    // extract the sensor
    sensorid = atoi(get_regex_match_string(msg->topic, &matches[1], subtopic));
    if (sensorid == 0) {
        return;
    }
    struct RadiatorSensor *sensor = find_sensor(sensorid);
    if (!sensor) {
        return;
    }

    // extract the subtopic
    get_regex_match_string(msg->topic, &matches[2], subtopic);

    // extract the value
    memset(value, 0, sizeof(value));
    strncpy(value, msg->payload, MIN(msg->payloadlen, sizeof(value) - 1));

    // handle subtopics
    if (!strcmp(subtopic, "desired_temperature")) {
        sensor->desiredTemperature = atof(value);
        sensor->desiredTemperatureTxStamp = 0;

    } else if (!strcmp(subtopic, "locate_sensor") && atoi(value)) {
        sensor->locate = 1;
    }
}


void heating_mosquitto_message_boiler(const struct mosquitto_message * msg) {
    char value[256];

    memset(value, 0, sizeof(value));
    strncpy(value, msg->payload, MIN(msg->payloadlen, sizeof(value) - 1));

    if (!strcmp(msg->topic, "/boiler/pair")) {
        boiler_pairing_code = atoi(value);
        boiler_pair_now = true;

    } else if (!strcmp(msg->topic, "/boiler/desired_state")) {
        boiler_state = atoi(value);
    }
}


void heating_mosquitto_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message * msg) {
    if (!strncmp(msg->topic, "/radiator/", 10)) {
        heating_mosquitto_message_radiator(msg);
    } else if (!strncmp(msg->topic, "/boiler/", 8)) {
        heating_mosquitto_message_boiler(msg);
    }
}


void heating_mosquitto_publish_double(struct mosquitto *mosq, uint32_t sensorid, char *subtopic, double d) {
    char topic[256];
    char strvalue[256];

    sprintf(strvalue, "%f", d);
    sprintf(topic, "/radiator/%i/%s", sensorid, subtopic);
    mosquitto_publish(mosq, NULL, topic, strlen(strvalue), strvalue, 0, true);
}


void heating_mosquitto_publish_int(struct mosquitto *mosq, uint32_t sensorid, char *subtopic, int i) {
    char topic[256];
    char strvalue[256];

    sprintf(strvalue, "%i", i);
    sprintf(topic, "/radiator/%i/%s", sensorid, subtopic);
    mosquitto_publish(mosq, NULL, topic, strlen(strvalue), strvalue, 0, true);
}


void heating_mosquitto_subscribe(struct mosquitto *mosq, uint32_t sensorid, char *subtopic) {
    char topic[256];

    sprintf(topic, "/radiator/%i/%s", sensorid, subtopic);
    mosquitto_subscribe(mosq, NULL, topic, 0);
}

void boiler_mosquitto_publish_int(struct mosquitto *mosq, int i) {
    char topic[256];
    char strvalue[256];

    sprintf(strvalue, "%i", i);
    mosquitto_publish(mosq, NULL, "/boiler/state", strlen(strvalue), strvalue, 0, true);
}


struct mosquitto *mosquitto_init(char *mosq_host, int mosq_port) {
    int err;
    struct mosquitto *mosq;

    if (err = mosquitto_lib_init()) {
        fprintf(stderr, "Faied to initialise mosquitto library\n");
        return NULL;
    }

    if (NULL == (mosq = mosquitto_new("heating", false, NULL))) {
        fprintf(stderr, "Faied to create mosquitto instance\n");
        return NULL;
    }
    mosquitto_message_callback_set(mosq, &heating_mosquitto_message_callback);

    if (err = mosquitto_connect_async(mosq, mosq_host, mosq_port, 30)) {
        fprintf(stderr, "Faied to connect to mosquitto broker\n");
        return NULL;
    }
    return mosq;
}


int main(int argc, char *argv[]) {
    int opt;
    int err;
    struct mosquitto *mosq;
    char *mosquitto_broker;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <mosquitto broker>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    mosquitto_broker = argv[1];

    if (initHardware()) {
        fprintf(stderr, "Cannot initialize BCM2835.\n");
        exit(EXIT_FAILURE);
    }

    if (!(mosq = mosquitto_init(mosquitto_broker, 1883))) {
        fprintf(stderr, "Cannot initialize mosquitto.\n");
        exit(EXIT_FAILURE);
    }

    // compile topic regex
    if (err = regcomp(&topic_regex, "/radiator/([0-9]+)/([a-z_]+)", REG_EXTENDED)) {
        fprintf(stderr, "Cannot setup regex.\n");
        exit(EXIT_FAILURE);
    }

    // start the mosquitto thread
    if (err = mosquitto_loop_start(mosq)) {
        fprintf(stderr, "Cannot start mosquitto thread.\n");
        exit(EXIT_FAILURE);
    }

    // subscribe to boiler topics
    mosquitto_subscribe(mosq, NULL, "/boiler/pair", 0);
    mosquitto_subscribe(mosq, NULL, "/boiler/desired_state", 0);

    // main loop
    while(1) {
        // energenie stuff
        struct RadiatorSensor *sensor = energenie_loop();
        if (NULL != sensor) {
            heating_mosquitto_publish_int(mosq, sensor->sensorid, "locate_sensor", 0);
            heating_mosquitto_publish_double(mosq, sensor->sensorid, "temperature", sensor->temperature);
            if (sensor->voltage) {
                heating_mosquitto_publish_double(mosq, sensor->sensorid, "voltage", sensor->voltage);
            }
            heating_mosquitto_publish_int(mosq, sensor->sensorid, "last_rx_stamp", sensor->lastRxStamp);

            if (!sensor->mqtt_setup) {
                heating_mosquitto_subscribe(mosq, sensor->sensorid, "locate_sensor");
                heating_mosquitto_subscribe(mosq, sensor->sensorid, "desired_temperature");
                sensor->mqtt_setup = 1;
            }
        }

        // salus boiler stuff
        if (0 != boiler_pairing_code) {
            if (boiler_pair_now) {
                bcm2835_spi_chipSelect(CS_868MHZ);
                txSalusPairing(boiler_pairing_code);
                bcm2835_spi_chipSelect(CS_433MHZ);

                boiler_pair_now = false;
            }

            if ((time(NULL) - boiler_tx_stamp) > BOILER_TX_SECS) {
                bcm2835_spi_chipSelect(CS_868MHZ);
                if (boiler_desired_state) {
                    txSalusBoilerOn(boiler_pairing_code);
                } else {
                    txSalusBoilerOff(boiler_pairing_code);
                }
                bcm2835_spi_chipSelect(CS_433MHZ);

                boiler_state = boiler_desired_state;
                boiler_mosquitto_publish_int(mosq, boiler_state);

                boiler_tx_stamp = time(NULL);
            }
        }
    }

    shutdownHardware();
}
