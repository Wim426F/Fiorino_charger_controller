#ifndef GLOBALS_H
#define GLOBALS_H

//#define Serial1 Serial // uncomment for test mode

// libraries
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SD.h>
#include <SPI.h>
#include <elapsedMillis.h>
#include <EEPROM.h>
#include <CAN.h>

/* GPIO */
//#define HARDWARE_VERSION_1 // uncomment to use pinout for old version

#ifndef HARDWARE_VERSION_1
#define HARDWARE_VERSION_2
#endif

#ifdef HARDWARE_VERSION_1
#define INPUT_S1 GPIO_NUM_36
#define CHARGER_ON GPIO_NUM_39
#define UART_RX1 GPIO_NUM_35
#define UART_TX1 GPIO_NUM_33
#define PWM GPIO_NUM_26

// not used
#define OUTPUT_S1 GPIO_NUM_13
#define OUTPUT_S2 GPIO_NUM_13
#define EVSE_PROX GPIO_NUM_13
#define EVSE_PILOT GPIO_NUM_13
#define EVSE_STATE_C GPIO_NUM_13
#define CAN_TX GPIO_NUM_13
#define CAN_RX GPIO_NUM_13
#define HB_IN1 GPIO_NUM_13
#define HB_IN2 GPIO_NUM_13
#define SD_CS GPIO_NUM_13
#endif

#ifdef HARDWARE_VERSION_2
#define INPUT_S1 GPIO_NUM_39
#define S1_LED_GREEN GPIO_NUM_17 // green led in evse socket
#define S2_PTC_ON GPIO_NUM_4   // solid state relais for ptc heater

#define EVSE_PROX GPIO_NUM_33    // input
#define EVSE_PILOT GPIO_NUM_35   // input
#define EVSE_STATE_C GPIO_NUM_16  // output

#define CAN_TX GPIO_NUM_27
#define CAN_RX GPIO_NUM_25

#define UART_RX1 GPIO_NUM_26
#define UART_TX1 GPIO_NUM_22

#define HB_IN1 GPIO_NUM_32 // H-bridge io 1
#define HB_IN2 GPIO_NUM_21 // H-bridge io 2

#define SD_CS GPIO_NUM_13
#define PWM GPIO_NUM_2 // PWM output to charger
#endif

extern File logfile;
extern File serialfile;
extern int logfile_nr;

/* Variables */
extern float celltemp;
extern float stateofcharge;
extern float dc_amps;
extern float vmin;
extern float vmax;
extern float vtot;
extern float balanced_capacity;
extern float balancing_power;

extern String str_vmin;
extern String str_vmax;
extern String str_vtot;
extern String str_ctmp;
extern String str_soc;
extern String str_dc_amps;
extern String str_max_evse_amps;
extern String str_max_cable_amps;
extern String str_ptc_temp;
extern String str_ptc_temp_sp;
extern String str_heating_en;

extern uint8_t input_s1;

extern long time_minutes;
extern long time_seconds;
extern unsigned long time_millis;
extern bool request_t;
extern const uint8_t chargerpwm_ch;
extern const uint8_t greenled_ch;
extern const uint8_t ptc_ch;

#endif