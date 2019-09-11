#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <bcm2835.h>
#include <mosquitto.h>
#include <string.h>
#include <regex.h>
#include "energenie.h"
#include "hw.h"
#include "radio.h"


regex_t topic_regex;

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


void mosq_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message * msg) {
    int err;
    regmatch_t matches[3];
    char subtopic[256];
    char value[256];
    uint16_t sensorid;

    fprintf(stderr, "mqtt: %s (%i)\n", msg->topic, msg->payloadlen);

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
    strncpy(msg->payload, value, MIN(msg->payloadlen, sizeof(value) - 1));

    // handle subtopics
    if (!strcmp(subtopic, "desired_temperature")) {
        sensor->desiredTemperature = atof(value);
        sensor->desiredTemperatureTxStamp = 0;

    } else if (!strcmp(subtopic, "locate_sensor") && atoi(value)) {
        sensor->locate = 1;
    }
}


void publish_double(struct mosquitto *mosq, uint32_t sensorid, char *subtopic, double d) {
    char topic[256];
    char strvalue[256];

    if (d == (int) d) {
        sprintf(strvalue, "%i", d);
    } else {
        sprintf(strvalue, "%f", d);
    }
    sprintf(topic, "/radiator/%i/%s", sensorid, subtopic);
    mosquitto_publish(mosq, NULL, topic, strlen(strvalue), strvalue, 0, true);
}


struct mosquitto *mosquitto_init(char *mosq_host, int mosq_port) {
    int err;
    struct mosquitto *mosq;

    if (err = mosquitto_lib_init()) {
        // FIXME: error handling
        return NULL;
    }

    if (NULL == (mosq = mosquitto_new("heating", false, NULL))) {
        // FIXME: error handling
        return NULL;
    }
    mosquitto_message_callback_set(mosq, &mosq_message_callback);

    if (err = mosquitto_connect_async(mosq, mosq_host, mosq_port, 30)) {
        // FIXME: error handling
        return NULL;
    }
    return mosq;
}


int main(int argc, char *argv[]) {
    int opt;
    int err;
    struct mosquitto *mosq;
    char topic[256];

    // parse args
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        switch (opt) {
        case 'i':
            // identifySensorId = atoi(optarg);
            // fprintf(stderr, "Waiting for sensor %i to appear...\n", identifySensorId);
            break;

        default:
            fprintf(stderr, "Usage: %s [-i sensorid to identify]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (initHardware()) {
        fprintf(stderr, "Cannot initialize BCM2835\n.");
        exit(EXIT_FAILURE);
    }

    if (!(mosq = mosquitto_init("beyond", 1883))) {
        fprintf(stderr, "Cannot initialize mosquitto\n.");
        exit(EXIT_FAILURE);
    }

    // compile topic regex
    if (err = regcomp(&topic_regex, "/radiator/([0-9]+)/([a-z]+)", REG_EXTENDED)) {
        // FIXME: error handling
    }

    // start the mosquitto thread
    mosquitto_loop_start(mosq);

    // deal with energenie messages
    while(1) {
        // check for energenie messages
        struct RadiatorSensor *sensor = energenie_loop(100);
        if (NULL == sensor) {
            continue;
        }

        publish_double(mosq, sensor->sensorid, "locate_sensor", 0);
        publish_double(mosq, sensor->sensorid, "temperature", sensor->temperature);
        publish_double(mosq, sensor->sensorid, "last_rx_stamp", time(NULL));
        publish_double(mosq, sensor->sensorid, "voltage", sensor->temperature);

        if (!sensor->mqtt_setup) {
            sprintf(topic, "/radiator/%i/locate", sensor->sensorid);
            mosquitto_subscribe(mosq, NULL, topic, 0);

            sprintf(topic, "/radiator/%i/desired_temperature", sensor->sensorid);
            mosquitto_subscribe(mosq, NULL, topic, 0);
        }
    }

    shutdownHardware();
}
