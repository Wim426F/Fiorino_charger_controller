#include <web_server.h>
#include <charger.h>

const char *ap_ssid = "Fiorino";
const char *password = "3VLS042020";
const char *ota_hostname = "FIORINO_ESP32";
const char *dns_hostname = "fiorino";

const char *PARAM_1 = "function";
const char *PARAM_2 = "command";
bool client_conn = false;

StaticJsonDocument<200> jstring; // Allocate a static JSON document

bool webserver_active = false;
bool wifiserial_active = false;
unsigned long since_web_req = 0;

MDNSResponder mdns;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
IPAddress ap_ip(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);

int available_networks = 0;
int wifi_timeout = 0;

void StartMdnsService()
{
  mdns.addService("http", "tcp", 80);

  if (!mdns.begin(dns_hostname))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  else
  {
    Serial.println((String) "mdns: http://" + dns_hostname + ".local");
  }
}

void ConfigWebServer()
{
  /* Webpages */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/html/index.html", FILE_READ); // read file from filesystem
              request->send(page, "/index.html", "text/html", false);
              wifiserial_active = false;
              since_web_req = time_minutes;
            });
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/html/update.html", FILE_READ); // read file from filesystem
              request->send(page, "/update", "text/html");
              page.close();
              wifiserial_active = false;
              since_web_req = time_minutes;
            });
  server.on("/parameters", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/html/parameters.html", FILE_READ);
              request->send(page, "/parameters.html", "text/html");
              page.close();
              wifiserial_active = false;
              since_web_req = time_minutes;
            });
  server.on("/serialport", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/html/serialport.html", FILE_READ);
              request->send(page, "/serialport.html", "text/html");
              String inputparam;
              // GET input1 value on fiorino.local/serialport?function=<>
              if (request->hasParam(PARAM_2))
              {
                inputparam = request->getParam(PARAM_2)->value();
                Serial.println(inputparam);
              }
              page.close();
              //wifiserial_active = true; 
              since_web_req = time_minutes;
            });
  server.on("/datalog", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/html/datalog.html", FILE_READ);
              request->send(page, "/datalog", "text/html", false);
              String inputparam;
              // GET input1 value on fiorino.local/datalog?function=<delete>
              if (request->hasParam(PARAM_1))
              {
                inputparam = request->getParam(PARAM_1)->value();
              }
              if (inputparam == "delete")
              {
                Serial.println("file deleted");
                dataLogger("clear");
                request->send(200);
              }
              page.close();
              wifiserial_active = false;
              since_web_req = time_minutes;
            });
  server.on("/logfile", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_READ);
              request->send(page, "/logfile", "text/plain", false);
              page.close();
              since_web_req = time_minutes;
              wifiserial_active = false;
            });
  server.on("/serial", HTTP_GET, [](AsyncWebServerRequest *request) // serial buffer file
            {
              File page = SD.open("/log/serial.txt", FILE_READ);
              request->send(page, "/serial", "text/plain", false);
              page.close();
              since_web_req = time_minutes;
              //wifiserial_active = true;
            });
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
            { esp_restart(); });

  /* Global files */
  server.on("/gauge.js", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/js/gauge.min.js", FILE_READ);
              request->send(page, "/gauge.js", "text/javascript");
            });
  server.on("/highcharts.js", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/js/highcharts.js", FILE_READ);
              request->send(page, "/highcharts.js", "text/javascript");
            });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/html/style.css", FILE_READ);
              request->send(page, "/style.css", "text/css");
              page.close();
            });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              File page = SD.open("/img/favicon.ico", FILE_READ);
              request->send(page, "/favicon.ico", "image/vnd.microsoft.icon");
            });
  /* XHTTP requests */
  server.on("/bms", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              since_web_req = time_minutes;

              DynamicJsonDocument doc(220);
              doc["vtot"] = str_vtot;
              doc["vmax"] = str_vmax;
              doc["vmin"] = str_vmin;
              doc["temp"] = str_ctmp;
              doc["amps"] = str_dc_amps;
              doc["soc"] = str_soc;
              doc["pwm"] = String(charger_pwm, 0);

              String jtostring;
              serializeJson(doc, jtostring);
              request->send_P(200, "text/plain", jtostring.c_str());
              since_web_req = time_minutes;
              wifiserial_active = false;
            });
  server.on("/evse", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              since_web_req = time_minutes;

              DynamicJsonDocument doc(140);

              doc["mca"] = str_max_cable_amps;
              doc["mea"] = str_max_evse_amps;

              String jtostring;
              serializeJson(doc, jtostring);
              request->send_P(200, "text/plain", jtostring.c_str());
              since_web_req = time_minutes;
              wifiserial_active = false;
            });
  server.on("/params", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              since_web_req = time_minutes;

              DynamicJsonDocument doc(300);
              doc["_1"] = String(CHARGER_MAX_AC_AMPS, 1);
              doc["_2"] = String(VMIN_LIM, 3);
              doc["_3"] = String(VMAX_LIM_LOWER, 3);
              doc["_4"] = String(VMAX_LIM_UPPER, 3);
              doc["_5"] = String(CELLTEMP_MIN, 1);
              doc["_6"] = String(CELLTEMP_MIN_UPPER, 1);
              doc["_7"] = String(CELLTEMP_MAX, 1);

              String jtostring;
              serializeJson(doc, jtostring);
              request->send_P(200, "text/plain", jtostring.c_str());
              since_web_req = time_minutes;
              wifiserial_active = false;
            });
  server.begin();
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  //Handle upload
  Serial.println("----UPLOAD-----");
  Serial.print("FILENAME: ");
  Serial.println(filename);
  Serial.print("INDEX: ");
  Serial.println(index);
  Serial.print("LENGTH: ");
  Serial.println(len);
  AsyncWebHeader *header = request->getHeader("X-File-Size");
  Serial.print("File size: ");
  Serial.println((size_t)header->value().toFloat());
  if (!Update.isRunning())
  {
    Serial.print("Status Update.begin(): ");
    Serial.println(Update.begin((size_t)header->value().toFloat()));
    Serial.print("Update remaining: ");
    Serial.println(Update.remaining());
  }
  else
  {
    Serial.println("Status Update.begin(): RUNNING");
  }
  Serial.print("FLASH BYTES: ");
  Serial.println(Update.write(data, len));
  Serial.print("Update remaining: ");
  Serial.println(Update.remaining());

  if (final)
  {
    Update.end();
    Serial.print("----FINAL-----");
  }
}

/* Websockets */
void notifyClients()
{
  //ws.textAll();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    sendWsData();
  }

  DeserializationError error = deserializeJson(jstring, data); // deserialize incoming Json String
  if (error)
  { // Print error msg if incoming String is not JSON formatted
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }
  const String pin_stat = jstring["PIN_Status"]; // String variable tha holds LED status
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void sendWsData()
{
  DynamicJsonDocument doc(200);
  doc["vtot"] = str_vtot;
  doc["vmax"] = str_vmax;
  doc["vmin"] = str_vmin;
  doc["temp"] = str_ctmp;
  doc["amps"] = str_dc_amps;
  doc["soc"] = str_soc;
  size_t len = measureJson(doc);
  AsyncWebSocketMessageBuffer *buffer = ws.makeBuffer(len); //  creates a buffer (len + 1) for you.
  if (buffer)
  {
    //serializeJson(doc, Serial);
    ws.textAll(buffer);
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String &var)
{
  Serial.println(var);
  if (var == "STATE")
  {
    if (true) // ledstate
    {
      return "ON";
    }
    else
    {
      return "OFF";
    }
  }
}

void cleanUpWs()
{
  ws.cleanupClients();
}
