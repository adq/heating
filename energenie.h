#ifndef ENERGENIE_H
#define ENERGENIE_H 1

#include <stdint.h>
#include <time.h>

struct RadiatorSensor {
    uint32_t sensorid;

    double temperature;

    double desiredTemperature;
    time_t desiredTemperatureTxStamp;

    double voltage;
    time_t voltageRxStamp;

    time_t lastRxStamp;

    uint8_t exercise_valve:1;
    uint8_t locate:1;
    uint8_t mqtt_setup:1;

    struct RadiatorSensor *next;
};

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

#define OOK_MSG_ADDRESS_LENGTH 10

void seedcrypt(uint16_t *ran, uint8_t pid, uint16_t pip);
uint8_t cryptbyte(uint16_t *ran, uint8_t dat);
int16_t crc(uint8_t const mes[], unsigned char siz);
int decodeValue(uint8_t** buf, int buflen, double *outNumber);
void tx(uint8_t *msg, uint8_t msglen);
void txRequestVoltage(uint32_t sensorid);
void txDesiredTemperature(uint32_t sensorid, uint8_t desiredTemperature);
void txIdentify(uint32_t sensorid);
void txExercise(uint32_t sensorid);
void txOOKSwitch(uint32_t house_address, uint32_t device_address, uint8_t on_off);
double decodeDouble(uint8_t *buf, int buflen);
struct RadiatorSensor *energenie_loop();
struct RadiatorSensor *find_sensor(uint32_t sensorid);

#endif
