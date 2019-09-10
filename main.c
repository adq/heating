#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <bcm2835.h>
#include <mosquitto.h>
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
    uint16_t sensorid;

    if (err = regexec(&topic_regex, msg->topic, 3, matches, 0)) {
        fprintf(stderr, "Failed to match topic %s\n", msg->topic);
        return;
    }
    sensorid = atoi(get_regex_match_string(msg->topic, &matches[1], subtopic));
    get_regex_match_string(msg->topic, &matches[2], subtopic);

    // ok, find the sensor
    struct RadiatorSensor *sensor = findSensor(sensorid);
    if (!sensor) {
        return;
    }

    // FIXME: extract supplied value

    // fixme deal with incoming subtopics
    if (!strcmp(subtopic, "desired_temperature")) {
        sensor->desiredTemperature = ??;
        sensor->temperatureTxStamp = 0;
        // FIXME

    } else if (!strcmp(subtopic, "locate_sensor")) {
        sensor->locate = true;
    }
}


struct mosquitto *mosquitto_init() {
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

    if (err = mosquitto_connect(mosq, mosq_host, mosq_port, 30)) {
        // FIXME: error handling
        return NULL;
    }
    return mosq;
}


int main(int argc, char *argv[]) {
    uint32_t identifySensorId = 0;
    int opt;
    int err;
    struct mosquitto *mosq;


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

    if (initHardware()) {
        fprintf(stderr, "Cannot initialize BCM2835\n.");
        exit(EXIT_FAILURE);
    }

    if (!(mosq = mosquitto_init())) {
        fprintf(stderr, "Cannot initialize mosquitto\n.");
        exit(EXIT_FAILURE);
    }

    if (err = regcomp(&topic_regex, "/radiator/([0-9]+)/([a-z]+)", REG_EXTENDED)) {
        // FIXME: error handling
    }

    while(1) {
        // check for energenie messages
        energenie_loop(mosq, 10);

        // check for mosquitto messages
        mosquitto_loop(mosq, 10, 1);
    }

    shutdownHardware();
}
