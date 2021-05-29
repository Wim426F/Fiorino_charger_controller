#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <globals.h>

/* Connectivity */
const char *ap_ssid = "Fiorino";
const char *password = "3VLS042020";
const char *ota_hostname = "FIORINO_ESP32";
const char *dns_hostname = "fiorino";

void StartMdnsService();
void ConfigWebServer();
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

extern IPAddress ap_ip;
extern IPAddress gateway;
extern IPAddress subnet;

extern int ssid_hidden;
extern int max_connection;
extern int available_networks;
extern int wifi_timeout;


#endif