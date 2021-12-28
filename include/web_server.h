#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <globals.h>

/* Connectivity */
extern IPAddress ap_ip;
extern IPAddress gateway;
extern IPAddress subnet;

void StartMdnsService();
void ConfigWebServer();
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
String processor(const String& var);

extern const char *ap_ssid;
extern const char *password;
extern const char *ota_hostname;
extern const char *dns_hostname;

extern bool webserver_active;
extern unsigned long since_web_req;

extern int ssid_hidden;
extern int max_connection;
extern int available_networks;
extern int wifi_timeout;


#endif