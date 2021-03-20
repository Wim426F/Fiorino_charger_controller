#ifndef GLOBALS_H
#define GLOBALS_H

// libraries
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SD.h>
#include <SPI.h>
#include <elapsedMillis.h>
#include <EEPROM.h>

extern File logfile;
extern int logfile_nr;

extern long int1;

extern String str_vmin;
extern String str_vmax;
extern String str_vtot;
extern String str_ctmp;
extern String str_soc;
extern String str_lem;

/* Variables */
extern float celltemp;
extern float stateofcharge;
extern float lemsensor;
extern float vmin;
extern float vmax;
extern float vtot;
extern float balanced_capacity;
extern float balancing_power;
extern float status;
extern float charger_duty;

extern bool endofcharge;
extern bool evse_on;
extern bool charge_limited;
extern bool is_balancing;
extern bool trickle_phase;

extern long time_minutes;
extern const uint8_t chargerpwm_ch;

#endif