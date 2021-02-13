// libraries
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncWebSocket.h>
#include <SD.h>
#include <SPI.h>
#include <elapsedMillis.h>
#include <EEPROM.h>

// headers
#include <credentials.h>

// cpp std
using namespace std;
#include <string>
#include <algorithm>

void task_core0(void *pvParameters);
void StartMdnsService();
void StartWebServer();
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
//void handleUploadWebpage(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUploadWebpage(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void SendSocketData();
void dataLogger(string event = "normal");
string GetSerialData(string input = "t");
void ControlCharger();
void LockEvse(bool lock_evse);

File logfile;
int logfile_nr = EEPROM.readInt(0);

/* Timers */
elapsedMillis since_int1 = 0;
elapsedMillis since_int2 = 0;
const long int1 = 10000;  //bms request interval
const long int2 = 120000; // every 2 minutes
int rx_timeout = 0;
int counter;
long time_minutes = 0;

/* Variables */
string status_str;
string output;

float celltemp = 0;
float stateofcharge = 0;
float lemsensor = 0;
float vmin = 0;
float vmax = 0;
float vtot = 0;
float dissipated_energy = 0;
float status = 0;
float charger_duty = 0;

/* Limits */
const float CELLTEMP_MIN = 0.0;
const float CELLTEMP_PREFERRED = 10.0;
const float CELLTEMP_MAX = 45.0;

const float VMIN_LIM = 3.000;
const float VMAX_LIM_LOWER = 4.000;
const float VMAX_LIM_UPPER = 4.150;
const float WH_DISSIPATED_MAX = 2000.00;

/* Triggers */
bool endofcharge = false;
bool evse_on = false;
bool soclim = false;

/* Connectivity */
extern const char *sta_ssid;
extern const char *sta_password;
extern const char *ap_ssid;
extern const char *ap_password;
extern const char *ota_hostname;
extern const char *ota_password;
extern const char *dns_hostname;
extern const char *http_username;
extern const char *http_password;

const char *PARAM_INPUT_1 = "function";

IPAddress local_ip(192, 168, 1, 173);
IPAddress ap_ip(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);
MDNSResponder mdns;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient *globalClient = NULL;

int ssid_hidden = 0;
int max_connection = 3;
int available_networks = 0;
int wifi_timeout = 0;

typedef int32_t esp_err_t;
typedef int uart_port_t;
//#define Serial1 Serial

/* GPIO */
#define CHARGER_LIMITED GPIO_NUM_36
#define EVSE GPIO_NUM_39
#define RX1 GPIO_NUM_35
#define TX1 GPIO_NUM_33

/* PWM channels */
const uint8_t chargerpwm_ch = 1;
const uint8_t lock_low = 2;
const uint8_t lock_high = 3;
const uint8_t unlock_low = 4;
const uint8_t unlock_high = 5;

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);
  Serial1.setRxBufferSize(4096);
  SD.begin(SS, SPI, 80000000, "/sd", 20);
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(local_ip, gateway, subnet);
  WiFi.begin(sta_ssid, sta_password);
  WiFi.setAutoReconnect(true);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password, ssid_hidden, max_connection);
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wifi connected to: " + (String)sta_ssid);
  }
  //Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();
  StartMdnsService();
  StartWebServer();

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO);
  esp_sleep_enable_uart_wakeup(0);
  esp_sleep_enable_uart_wakeup(1);
  esp_err_t gpio_set_pull_mode(gpio_num_t EVSE, gpio_pull_mode_t GPIO_PULLDOWN_ONLY);
  esp_err_t gpio_set_pull_mode(gpio_num_t CHARGER_LIMITED, gpio_pull_mode_t GPIO_PULLDOWN_ONLY);
  ledcSetup(chargerpwm_ch, 1000, 10); // channel, freq, res
  ledcSetup(lock_high, 1000, 8);
  ledcSetup(lock_low, 1000, 8);
  ledcSetup(unlock_high, 1000, 8);
  ledcSetup(unlock_low, 1000, 8);
  ledcAttachPin(GPIO_NUM_26, chargerpwm_ch);
  ledcAttachPin(GPIO_NUM_21, lock_high);
  ledcAttachPin(GPIO_NUM_22, lock_low);
  ledcAttachPin(GPIO_NUM_16, unlock_high);
  ledcAttachPin(GPIO_NUM_17, unlock_low);
  delay(1000);
  if (GetSerialData("t") == "Succes")
  {
    ControlCharger();
  }
}

void loop()
{
  ArduinoOTA.handle();

  time_minutes = millis() / 60000; // time in minutes

  evse_on = gpio_get_level(EVSE);
  evse_on = !evse_on;
  soclim = gpio_get_level(CHARGER_LIMITED);

  if (since_int1 > int1)
  {
    Serial.println("\n\nGetting Data");
    if (GetSerialData("t") == "Succes")
    {
      ControlCharger();
    }
    else
    {
      charger_duty = 0;
      if (vmax >= 4.15 && stateofcharge == 70)
      {
        dataLogger("endofcharge");
        esp_light_sleep_start();
      }
    }
    since_int1 = since_int1 - int1;
  }

  if (since_int2 > int2)
  {
    dataLogger();
    since_int2 -= int2;
  }
  /*
  if (evse_on == true)
  {
    LockEvse(true);
  }
  else
  {
    LockEvse(false);
  } */
}

inline float stof(const string &_Str, size_t *_Idx = nullptr) // convert string to float
{
  int &_Errno_ref = errno; // Nonzero cost, pay it once
  const char *_Ptr = _Str.c_str();
  char *_Eptr;
  _Errno_ref = 0;
  const float _Ans = strtof(_Ptr, &_Eptr);

  if (_Idx)
  {
    *_Idx = static_cast<size_t>(_Eptr - _Ptr);
  }

  return _Ans;
}

void dataLogger(string event)
{
  if (!SD.exists("/html/index.html"))
  {
    Serial.println("SD card removed!");
    SD.end();
    SD.begin(SS, SPI, 80000000, "/sd", 20);
    delay(500);
    if (SD.exists("/html/index.html"))
    {
      Serial.println("SD card reinitialized");
    }
    else
    {
      Serial.println("Failed to reinitialize SD card");
    }
  }

  if (SD.totalBytes() - SD.usedBytes() < 1000)
  {
    for (int i = 0; i < logfile_nr; i++)
    {
      SD.remove("/log/logfile_" + (String)i + ".txt");
    }
  }

  if (logfile.size() > 2000000) // max file size 2MB
  {
    EEPROM.writeInt(0, logfile_nr + 1); // new file location
    logfile.close();
    logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_WRITE);
  }

  if (!SD.exists("/log/logfile_0.txt"))
  {
    logfile = SD.open("/log/logfile_0.txt", FILE_WRITE);
    logfile.print("Time,Vmin,Vmax,Vtot,Temperature,SOC,LEM,ChargerPwm\n");
    EEPROM.writeInt(0, 0);
  }

  if (event.compare("endofcharge") == 0)
  {
    logfile.print("\nCharging Finished. Going to sleep now..\n\n");
    logfile.close();
  }

  if (vmin != 0 && vmax != 0 && vtot)
  {
    logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_APPEND);
    logfile.print((String)time_minutes + "," + vmin + "," + vmax + "," + vtot + "," + celltemp + "," + stateofcharge + "," + lemsensor + "," + (int)charger_duty + "\n");
  }
}

string GetSerialData(string input)
{
  string serial_str;
  serial_str.reserve(3500);

  while (Serial1.available() != -1 && rx_timeout < 10)
  {
    byte trash = Serial1.read();
    rx_timeout++; 
    delay(5);
  }

  if (input.length() < 2)
  {
    Serial1.println(input.c_str());
    Serial.println(input.c_str());
  }
  while (!Serial1.available() && rx_timeout <= 300)
  {
    rx_timeout++;
    delay(100);
  }
  if (rx_timeout >= 300)
  {
    rx_timeout = 0;
    Serial.println("No data received: timeout");
  }
  else
  {
    rx_timeout = 0;
    Serial.println("BMS data received!");

    while (Serial1.available() > 0)
    {
      serial_str += char(Serial1.read());
      delay(2); // very neccesary
    }

    remove(serial_str.begin(), serial_str.end(), ' ');
    //serial_str.resize(2040);

    if (input[0] == 't')
    {
      dissipated_energy = 0;
      size_t dissipated_energy_idx = 0;

      // Cell temperature
      size_t celltemp_idx = serial_str.find("-7f") + 21;
      if (serial_str[celltemp_idx] == '-')
        celltemp = stof(serial_str.substr(celltemp_idx, celltemp_idx + 4));
      if (serial_str[celltemp_idx] == '.')
        celltemp = stof(serial_str.substr(celltemp_idx, celltemp_idx + 2));
      if (serial_str[celltemp_idx] != '-' || serial_str[celltemp_idx] != '.')
        celltemp = stof(serial_str.substr(celltemp_idx, celltemp_idx + 4));

      // Total voltage
      size_t vtot_idx = serial_str.find("Vtot") + 5;
      vtot = 0;
      vtot = stof(serial_str.substr(vtot_idx, vtot_idx + 6));

      // SOC
      size_t stateofcharge_idx = serial_str.find("SOC") + 4;
      stateofcharge = 0;
      stateofcharge = stof(serial_str.substr(stateofcharge_idx, stateofcharge_idx + 4));

      // Current Sensor
      size_t lemsensor_idx = serial_str.find("LEM") + 4;
      lemsensor = 0;
      lemsensor = stof(serial_str.substr(lemsensor_idx, lemsensor_idx + 4));

      // Min and max cell voltages
      size_t vmin_idx = 0, vmax_idx = 0;
      if (serial_str.find("Vmin") != -1 && serial_str.find("Vmax") != -1)
      {
        vmin_idx = serial_str.find("Vmin:") + 5;
        vmax_idx = serial_str.find("Vmax:") + 5;
      }
      if (serial_str.find("Shunt") != -1) // Shunt is similar to vmax
      {
        vmin_idx = serial_str.find("Vmed:") + 5;
        vmax_idx = serial_str.find("Shunt:") + 6;
      }
      vmin = 0;
      vmax = 0;
      vmin = stof(serial_str.substr(vmin_idx, vmin_idx + 5));
      vmax = stof(serial_str.substr(vmax_idx, vmax_idx + 5));

      Serial.print("vmin: ");
      Serial.println(vmin);
      Serial.print("vmax: ");
      Serial.println(vmax);
      Serial.print("vtot: ");
      Serial.println(vtot);
      Serial.print("Temperature: ");
      Serial.println(celltemp);
      Serial.print("State of Charge: ");
      Serial.println(stateofcharge);
      Serial.print("Current sensor: ");
      Serial.println(lemsensor);
      output = "Succes";
    }

    if (input[0] == 'd')
    {
      size_t dissipated_energy_idx = serial_str.find("dissipata") + 9;
      dissipated_energy = stof(serial_str.substr(dissipated_energy_idx, dissipated_energy_idx + 6));
      output = "Succes";
    }
  }

  return output;
}

void ControlCharger()
{
  if (vmin < VMIN_LIM && vmin > 0)
  {
    charger_duty = 915 - (VMIN_LIM - vmin) * 1000 - 150;

    if (charger_duty < 100)
      charger_duty = 170;
  }

  if (vmin >= VMIN_LIM && vmax <= VMAX_LIM_LOWER)
    charger_duty = 910;

  if (vmin >= VMIN_LIM && vmax >= VMAX_LIM_LOWER && vmax <= VMAX_LIM_UPPER)
    charger_duty = (VMAX_LIM_UPPER - vmax) * 3800 + 100;

  if (vmax > VMAX_LIM_UPPER)
    charger_duty = 0;

  /* Temperature */

  if (celltemp < CELLTEMP_MIN)
    charger_duty = 0;

  if (celltemp < CELLTEMP_PREFERRED && celltemp > CELLTEMP_MIN) // throttle charger when temperature is lower
    charger_duty -= (CELLTEMP_PREFERRED - celltemp) * (charger_duty / 20);

  if (celltemp > CELLTEMP_MAX)
    charger_duty = 0;

  /* Balancing */
  if (dissipated_energy > WH_DISSIPATED_MAX)
    charger_duty = 0;

  if ((charger_duty > 0 && charger_duty < 100) || charger_duty < 0)
  {
    charger_duty = 0;
  }

  if (vmin == 0)
    charger_duty = 0;

  if (charger_duty != 0)
  {
    if (charger_duty < 170)
      charger_duty = 170;

    if (charger_duty > 915)
      charger_duty = 915;
  }
  ledcWrite(chargerpwm_ch, charger_duty);
}

void LockEvse(bool)
{
  if (true)
  {
    ledcWrite(unlock_high, 200); // switch P fet unlock off
    ledcWrite(unlock_low, 0);    // switch N fet unlock off
    delayMicroseconds(1);        // wait for fet delay & fall time
    ledcWrite(lock_high, 0);     // switch P fet lock on
    ledcWrite(lock_low, 200);    // switch N fet lock on
    delayMicroseconds(1);
  }
  if (false)
  {
    ledcWrite(lock_high, 200);  // turn P fet lock off
    ledcWrite(lock_low, 0);     // turn N fet lock off
    delayMicroseconds(1);       // wait for fet delay & fall time
    ledcWrite(unlock_high, 0);  // turn P fet unlock on
    ledcWrite(unlock_low, 200); // turn N fet unlock on
  }
}

void StartMdnsService()
{
  //set hostname
  if (!MDNS.begin(dns_hostname))
  {
    Serial.println("Error setting up MDNS responder!");
  }
  MDNS.addService("http", "tcp", 80);
}

void SendSocketData()
{
  String vmin_json = "{\"vmin\":";
  vmin_json += vmin;
  vmin_json += "}";

  String vmax_json = "{\"vmax\":";
  vmax_json += vmax;
  vmax_json += "}";

  String celltemp_json = "{\"celltemp\":";
  celltemp_json += celltemp;
  celltemp_json += "}";
}

String outputState(int output)
{
  if (evse_on)
  {
    return "checked";
  }
  else
  {
    return "";
  }
}

String processor(const String &var)
{
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER")
  {
    String buttons = "";
    buttons += "<p class=\"textstyle\">Temperature: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)celltemp;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Battery Current: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)lemsensor;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Battery Voltage: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)vtot;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Vmin: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)vmin;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Vmax: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)vmax;
    buttons += "</b>";

    buttons += "<p class=\"textstyle\">Charger Speed: </p>";
    buttons += "<b class=\"textstyle\">";
    buttons += (String)charger_duty;
    buttons += "</b>";

    //buttons += "<label class=\"button\"><input value=\"Download\" type=\"button\" oncick=\"editlog(this)\" id=\"download\" " + outputState(2) + "></label>";
    buttons += "<label class=\"button\"><input value=\"Delete\" type=\"button\" onclick=\"editlog(this)\" id=\"delete\" " + outputState(4) + "></label>";
    return buttons;
  }
  return String();
}

void StartWebServer()
{
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/index.html", "r"); // read file from filesystem
    request->send(page, "/index.html", "text/html", false, processor);
  });
  server
      .on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        File page = SD.open("/html/update.html", "r"); // read file from filesystem
        request->send(page, "/update", "text/html");
      })
      .setAuthentication(http_username, http_password);
  server.on("/parameters", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/parameters.html", "r");
    request->send(page, "/parameters.html", "text/html");
  });
  server.on("/datalog", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/datalog.html", "r");
    File logfile;
    request->send(page, "/datalog", "text/html", false, processor);
    String inputparam;
    // GET input1 value on fiorino.local/datalog?function=<delete>
    if (request->hasParam(PARAM_INPUT_1))
    {
      inputparam = request->getParam(PARAM_INPUT_1)->value();
    }
    if (inputparam == "delete")
    {
      logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", "w");
      logfile.print("");
      logfile.close();
    }
  });
  server.on("/logdownload", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", "r");
    request->send(page, "/logfile", "text/plain", true);
  });
  server.on("/logfile", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", "r");
    request->send(page, "/logfile", "text/plain", false);
  });
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/test.html", "r");
    request->send(page, "/test", "text/html", false, processor);
  });
  server.on("/pureknob.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/js/pureknob.js", "r");
    request->send(page, "/pureknob.js", "text/javascript");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/css/style.css", "r");
    request->send(page, "/style.css", "text/css");
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/etc/favicon.ico", "r");
    request->send(page, "/favicon.ico", "image/vnd.microsoft.icon");
  });
  server.begin();
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.println("WebSocket Client connected!");
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.println("Websocket Client disconnected!");
  }

  if (since_int2 > int2)
  {
    since_int2 = since_int2 - int2;
    if (globalClient != NULL && globalClient->status() == WS_CONNECTED)
    {
      uint8_t getVmin = globalClient->status();

      Serial.println(getVmin);
    }
    else
    {
      Serial.println("failed");
    }
  }
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

/* void handleUploadWebpage(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    request->_tempFile = SD.open(filename, "w");
  }
  if (request->_tempFile)
  {
    if (len)
    {
      request->_tempFile.write(data, len);
    }
    if (final)
    {
      request->_tempFile.close();
    }
  }
  File page = SD.open("/html/update.html", "r"); // read file from filesystem
  request->send(page, "/update", "text/html");
}
*/

void handleUploadWebpage(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if (!index)
  {
    Serial.printf("UploadStart: %s\n", filename.c_str());
  }
  for (size_t i = 0; i < len; i++)
  {
    Serial.write(data[i]);
  }
  if (final)
  {
    Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
  }
}
