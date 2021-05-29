#ifndef CHARGER_H
#define CHARGER_H

#include <string>

void ControlCharger(bool charger_on = true);
void dataLogger(std::string parameter = "");

/* EVSE */
void handleEvse();
void risingIRQ();
void fallingIRQ();

extern float evse_pilot_mvolt;
extern float evse_prox_mvolt;
extern int evse_pilot_pwm;
extern bool evse_ready;

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

#endif
