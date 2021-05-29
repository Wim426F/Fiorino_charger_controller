#ifndef UART_PARSER_H
#define UART_PARSER_H

#include <string>

/**
 * @brief	 Request data from BMS trough RS232
 * @return  "waiting", "succes", "timeout"
 */
std::string GetSerialData(std::string input); 

/**
 * @brief	 Parse data from to string to floats
 * @return  "succes" or "fail"
 */
std::string ParseStringData(std::string input);

extern std::string serial_string;

#endif