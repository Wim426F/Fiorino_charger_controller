#ifndef UART_PARSER_H
#define UART_PARSER_H

#include <string>

/**
 * @brief	 Request data from BMS trough RS232
 * @param    ""     Check state
 * @param    "t"    Request cell voltages from BMS
 * @param    "d"    Request cell balancing from BMS
 */
void GetSerialData(std::string input = "");

/**
 * @brief	 Parse data from to string to floats
 * @return  "succes" or "fail"
 */
std::string ParseStringData(std::string input);

extern int bms_req;
extern int car_mode;
extern std::string serial_string;
extern int rx_timeouts;

#endif