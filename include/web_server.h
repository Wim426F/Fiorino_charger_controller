#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <globals.h>

void StartMdnsService();
void StartWebServer();
String outputState(int output);
String processor(const String &var);

extern IPAddress local_ip;
extern IPAddress ap_ip;
extern IPAddress gateway;
extern IPAddress subnet;

extern int ssid_hidden;
extern int max_connection;
extern int available_networks;
extern int wifi_timeout;


#endif