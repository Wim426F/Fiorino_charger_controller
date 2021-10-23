#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <globals.h>

/* Connectivity */
extern AsyncWebSocket ws;
extern IPAddress ap_ip;
extern IPAddress gateway;
extern IPAddress subnet;

void StartMdnsService();
void ConfigWebServer();
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void notifyClients();
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len);
String processor(const String& var);
void initWebSocket();
void sendWsData();
void cleanUpWs();

extern const char *ap_ssid;
extern const char *password;
extern const char *ota_hostname;
extern const char *dns_hostname;

extern bool webserver_active;
extern bool wifiserial_active;
extern unsigned long since_web_req;

extern int ssid_hidden;
extern int max_connection;
extern int available_networks;
extern int wifi_timeout;


#endif