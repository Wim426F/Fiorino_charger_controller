#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <globals.h>

void StartMdnsService();
void StartWebServer();
void SendSocketData();
String outputState(int output);
String processor(const String &var);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

extern IPAddress local_ip;
extern IPAddress ap_ip;
extern IPAddress gateway;
extern IPAddress subnet;

extern int ssid_hidden;
extern int max_connection;
extern int available_networks;
extern int wifi_timeout;


#endif