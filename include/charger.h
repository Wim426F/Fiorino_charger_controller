#ifndef CHARGER_H
#define CHARGER_H

#include <string>

/**
 * @brief	 Log parameters to SD card, non default parameters are printed as comments
 * @param    "start" start a new logging session
 * @param    "finished" log if charging has finished
 * @param    "clear" clear last logging file
 */
void dataLogger(std::string parameter = "");

void ControlCharger(bool charger_on = true);

/* EVSE */
void getEvseParams();
void handleEvse();
void risingIRQ();
void fallingIRQ();

struct evse_s
{
    float pilot_mV;
    float prox_mV;
    float pilot_pwm;
    float max_cable_amps;
    float max_ac_amps;
    bool is_waiting;
    bool is_plugged_in;
    bool is_connected;
    unsigned long since_plugged_in;
    unsigned long since_enabled;
};
extern struct evse_s evse;

extern float charger_pwm;

/* Charging states */
extern bool endofcharge;
extern bool bms_is_balancing;
extern bool trickle_phase;

/* Limits */
extern const float CELLTEMP_MIN;
extern const float CELLTEMP_PREFERRED;
extern const float CELLTEMP_MAX;
extern const float VMIN_LIM;
extern const float VMAX_LIM_LOWER;
extern const float VMAX_LIM_UPPER;
extern const float VTOT_LOW;
extern const float VTOT_MAX;
extern const float WH_DISSIPATED_MAX;
extern const float CHARGER_MAX_AC_AMPS;

#endif
