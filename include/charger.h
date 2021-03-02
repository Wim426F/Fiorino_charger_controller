#ifndef CHARGER_H
#define CHARGER_H

#include <string>

void ControlCharger(bool charger_on = true);
std::string GetSerialData(std::string input = "t");
void dataLogger(std::string event = "normal");

/* Limits */
extern const float CELLTEMP_MIN;
extern const float CELLTEMP_PREFERRED;
extern const float CELLTEMP_MAX;
extern const float VMIN_LIM;
extern const float VMAX_LIM_LOWER;
extern const float VMAX_LIM_UPPER;
extern const float VTOT_LOW;
extern const float VTOT_MAX;
extern const float WH_DISSIPATED_MAX0;

#endif