#include <unistd.h>
#include <mosquitto.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define mosq_host "localhost"
#define mosq_port 1883


void mosq_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message * msg) {

}


regex_t topic_regex;

char *getmatch(char *source, regmatch_t* match, char *buf) {
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


int main(int argc, char *argv[]) {
    int err;
    struct mosquitto *mosq;
    regmatch_t matches[3];
    char tmp[256];

    printf("%i\n", time(NULL));

    // if (err = regcomp(&topic_regex, "/radiator/([0-9]+)/([a-z]+)", REG_EXTENDED)) {
    //     printf("COMPERRR: %i\n", err);
    //     return 0;
    //     // FIXME: error handling
    // }

    // if (err = regexec(&topic_regex, argv[1], 3, matches, 0)) {
    //     printf("EXECERRR: %i\n", err);
    //     return 0;
    //     // FIXME: error handling
    // }

    // printf("%s\n", getmatch(argv[1], &matches[1], tmp));
    // printf("%s\n", getmatch(argv[1], &matches[2], tmp));
    // printf("%i %i\n", matches[1].rm_so, matches[1].rm_eo);
    // printf("%i %i\n", matches[2].rm_so, matches[2].rm_eo);



/*
    if (err = mosquitto_lib_init()) {
        // FIXME: error handling
    }

    if (NULL == (mosq = mosquitto_new("heating", false, NULL))) {
        // FIXME: error handling
    }
    mosquitto_message_callback_set(mosq, &mosq_message_callback);

    if (err = mosquitto_connect_async(mosq, mosq_host, mosq_port, 30)) {
        // FIXME: error handling
    }
    mosquitto_loop_start(mosq);
*/

    return 0;
}
