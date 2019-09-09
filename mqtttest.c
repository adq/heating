#include <unistd.h>
#include <mosquitto.h>

#define mosq_host "localhost"
#define mosq_port 1883


void mosq_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message * msg) {

}


int main(int argc, char *argv[]) {
    int err;
    struct mosquitto *mosq;

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


    return 0;
}
