#include <can_parser.h>
#include <globals.h>

/* CAN-bus */
// Most cars support 11-bit adddress, others require 29-bit (extended) addressing
const bool useStandardAddressing = true;
const long can_baudrate = 125E3; // 125kbps

