<<<<<<< HEAD
#include <Arduino.h>
#ifdef TARGET_ESP32
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
//#include <Update.h>
#include <AsyncTCP.h>
#include <AsyncWebSocket.h>
#include <AsyncJson.h>
#include <SD.h>
#include <FS.h>
#include <SPI.h>
#include <analogWrite.h>
#include <elapsedMillis.h>
//#include "esp32_bt_music_receiver.h"

void StartMdnsService();
void StartWebServer();
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
//void handleUploadWebpage(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUploadWebpage(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void SendSocketData();
#endif
bool GetSerialData();
void ChargerControl();
void LockEvse(bool lock_evse);

elapsedMillis since_int1 = 0;
elapsedMillis since_int2 = 0;
const long int1 = 30000;
const long int2 = 1000;

/* Data */
String serial_str;
String celltemp_str;
String vmin_str;
String vmax_str;
String balcap_str;
char bms_cmd = 't';

int celltemp_idx = 0;
int vmin_idx = 0;
int vmax_idx = 0;
int balcap_idx = 0;
uint8_t bms_state = 0; // 0:not charging   1:charging   2:charging ready

float vmin = 0;
float vmax = 0;
float celltemp = 0;
float balcap = 0;
float charger_duty = 0;
float charger_speed = 0;

/* Limits */
const float celltemp_min = 0.0;
const float celltemp_max = 40.0;
const float tempdif_max = 10.0;
const float vmin_lim = 3.000;
const float vmax_lim_low = 4.000;
const float vmax_lim_upp = 4.150;
const float balcap_max = 2000.00;
int rx_timeout;

/* Triggers */
bool endofcharge = false;
bool evse_on = false;
bool voltage_lim = false;
bool bms_get_stat = true;

#ifdef TARGET_ESP32
/* Wifi */
const char *sta_ssid = "Boone-Huis";
const char *sta_password = "b00n3meubelstof@";
const char *ap_ssid = "Fiorino";
const char *ap_password = "3VLS042020";
const char *ota_hostname = "FIORINO_ESP32";
const char *ota_password = "MaPe1!";
const char *dns_hostname = "fiorino";
const char *http_username = "wim";
const char *http_password = "w1mb00n3";
int ssid_hidden = 0;
int max_connection = 3;
int available_networks = 0;
int wifi_timeout = 0;
IPAddress local_ip(192, 168, 1, 173);
IPAddress ap_ip(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);
MDNSResponder mdns;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocketClient *globalClient = NULL;



/* Inputs */
#define Serial1 Serial
const uint8_t evse_pin = D5;
const uint8_t charger_lim_pin = D6;

/* Outputs */
const uint8_t charger_pwm_pin = D0;
const uint8_t lock_plug_N = D1; // fet low side
const uint8_t lock_plug_P = D2; // fet high side
const uint8_t unlock_plug_N = D4;
const uint8_t unlock_plug_P = D3;

/*
// Bluetooth 
BluetoothA2DSink a2d_sink;
char *blde_name = "FIORINO_BT";

void BluetoothAudio()
{
  
  static const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,                         // corrected by info from bluetooth
      .bits_per_sample = (i2s_bits_per_sample_t)16, // the DAC module will only take the 8bits from MSB 
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S_MSB,
      .intr_alloc_flags = 0, // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false};

  static const i2s_pin_config_t i2s_pin_config = {
      .bck_io_num = 34,
      .ws_io_num = 35,
      .data_out_num = 33,
      .data_in_num = I2S_PIN_NO_CHANGE};
  
  a2d_sink.set_i2s_config(i2s_config);
  //a2d_sink.set_pin_config(i2s_pin_config);
  a2d_sink.start(blde_name);
}
*/
#endif

#ifdef TARGET_TEENSY40
/* Inputs */
const uint8_t evse_pin = 3;
const uint8_t charger_lim_pin = 4;

/* Outputs */
const uint8_t charger_pwm_pin = 5;
const uint8_t lock_plug_N = 9;  // fet low side
const uint8_t lock_plug_P = 10; // fet high side
const uint8_t unlock_plug_N = 11;
const uint8_t unlock_plug_P = 12;
#endif

void setup()
{
#ifdef TARGET_ESP32
  Serial.begin(115200);
  Serial1.begin(115200);
  serial_str.reserve(5000);
  SPI.begin(D5, D6, D7, D8);
  SD.begin(D8, SPI, 80000000);
  if (SD.begin() == 1)
  {
    Serial.println("SD card found!");
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.config(local_ip, gateway, subnet);
  WiFi.begin(sta_ssid, sta_password);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password, ssid_hidden, max_connection);
  while (WiFi.status() != WL_CONNECTED && wifi_timeout < 40)
  {
    delay(100);
    wifi_timeout++;
    if (wifi_timeout >= 10)
    {
      wifi_timeout = 0;
      Serial.println("WiFi connection not established!");
      break;
    }
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wifi connected to: " + (String)sta_ssid);
  }
  Serial.println(WiFi.localIP());
  // Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();
  StartMdnsService();
  StartWebServer();
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO);
  esp_sleep_enable_uart_wakeup(0);
  esp_sleep_enable_uart_wakeup(1);
  //BluetoothAudio();

  uint32_t usage = 100 - ((ESP.getFreeSketchSpace() * 100) / ESP.getFlashChipSize());
  Serial.println("Total Flash: " + (String)ESP.getFlashChipSize());
  Serial.println("Free Flash: " + (String)ESP.getFreeSketchSpace());
  Serial.println("Flash speed: " + (String)ESP.getFlashChipSpeed());
  Serial.println("Flash mode: " + (String)ESP.getFlashChipMode());
  Serial.println("Total RAM: " + (String)ESP.getPsramSize());
  Serial.println("Free RAM: " + (String)ESP.getFreePsram());
  Serial.println("Total Heap: " + (String)ESP.getHeapSize());
  Serial.println("Free Heap: " + (String)ESP.getFreeHeap());
  Serial.println("Sketch size usage (%): " + (String)usage);
  Serial.println("CPU clock (MHz): " + (String)ESP.getCpuFreqMHz());

  analogWriteResolution(10);
  analogWriteFrequency(1000);
  Serial.setRxBufferSize(4096);
  Serial1.setRxBufferSize(4096);
#endif
#ifdef TEENSY_40
  analogWriteFrequency(5, 1000);
  analogWriteResolution(10);
#endif

  pinMode(evse_pin, INPUT_PULLUP);
  pinMode(charger_lim_pin, INPUT_PULLUP);
  pinMode(charger_pwm_pin, OUTPUT);
  pinMode(lock_plug_N, OUTPUT);
  pinMode(lock_plug_P, OUTPUT);
  pinMode(unlock_plug_N, OUTPUT);
  pinMode(unlock_plug_P, OUTPUT);
  delay(500);
}

void loop()
{
#ifdef TARGET_ESP32
  ArduinoOTA.handle();
  if (WiFi.status() != WL_CONNECTED)
  {
    available_networks = WiFi.scanNetworks(sta_ssid, sta_ssid);
    if (available_networks == 1)
    {
      WiFi.begin(sta_ssid, sta_password);
    }
  }
  if (!SD.exists("/html"))
  {
    SD.begin(D8, SPI, 80000000);
  }

#endif

  evse_on = digitalRead(evse_pin);
  //evse_on = !evse_on;
  voltage_lim = digitalRead(charger_lim_pin);
  voltage_lim = !voltage_lim;
/*
  if (since_int1 > int1)
  {
    since_int1 = since_int1 - int1;
    GetSerialData();
  }

  if (GetSerialData() == true)
  {
    ChargerControl();
  }*/
  
  if (since_int2 > int2)
  {
    since_int2 = since_int2 - int2;
    Serial.println((String) "vmin: " + vmin + "   vmax: " + vmax + "   PWM: " + charger_duty);
    /*
    File logfile = SD.open("/log/logfile.txt");
    logfile.println(" ");
    logfile.println("--------------------");
    logfile.println(" ");
    logfile.close();
    */
  }

  if (Serial.available() > 0)
  {
    bms_cmd = Serial.read();
    if (bms_cmd == 't')
    {
      Serial.println(serial_str);
      delay(100);
    }
  }

  // if charging has finished put esp32 in light sleep
  if (vmax > 4.10 && GetSerialData() == false)
  {
    charger_duty = 0;
    endofcharge = true;
    //esp_light_sleep_start();
  }

  if (evse_on == true)
  {
    analogWrite(charger_pwm_pin, charger_duty);
    LockEvse(true);
  }
  else
  {
    LockEvse(false);
  }
}

bool GetSerialData()
{
  serial_str = "";
  bms_cmd = 't';
  Serial1.print(bms_cmd);
  Serial1.print('\r');
  Serial.println(bms_cmd);

  while (Serial1.available() == 0 && rx_timeout <= 15000)
  {
    rx_timeout++;
    delay(1);
  }
  if (rx_timeout >= 15000)
  {
    Serial.println("No data received: timeout");
    rx_timeout = 0;
  }
  while (Serial1.available() > 0)
  {
    delayMicroseconds(2); //delay to allow byte to arrive in input buffer
    serial_str += char(Serial1.read());
  }
  if (serial_str.length() > 0)
  {
    Serial.println("BMS data received!");
    serial_str.replace(" ", "");
    serial_str.replace("*", "");
    serial_str.replace("N.C.", "");

    if (serial_str.indexOf("vmin") > -1 && serial_str.indexOf("vmax") > -1)
    {
      bms_state = 0;
      vmin_idx = serial_str.indexOf("vmin:") + 5;
      vmax_idx = serial_str.indexOf("vmax:") + 5;
    }

    if (serial_str.indexOf("Shunt") > -1) // Shunt is similar to vmax
    {
      bms_state = 1;
      vmin_idx = serial_str.indexOf("Vmed:") + 5;
      vmax_idx = serial_str.indexOf("Shunt:") + 6;
    }

    celltemp_idx = serial_str.indexOf("SOC") - 5; // find the location of certain strings
    celltemp_str = serial_str.substring(celltemp_idx, celltemp_idx + 5);
    if (celltemp_str.startsWith(String('-')))
    {
      celltemp = celltemp_str.toFloat();
    }
    else
    {
      celltemp_str.remove(0, 1);
      celltemp = celltemp_str.toFloat();
    }

    vmin_str = serial_str.substring(vmin_idx, vmin_idx + 5);
    vmax_str = serial_str.substring(vmax_idx, vmax_idx + 5);
    vmin = vmin_str.toFloat(); // convert string to float
    vmax = vmax_str.toFloat();
    if (vmax > vmax_lim_low)
    {
    /*
    File logfile = SD.open("/log/logfile.txt");
    logfile.println(" ");
    logfile.println("--------------------");
    logfile.println(" ");
    logfile.println(serial_str);
    logfile.close();
    */
    }
  }
  if (bms_state > 1)
  {
    return false;
  }
  else
  {
    return true;
  }
}

void ChargerControl()
{
  if (vmin < vmin_lim)
  {
    charger_duty = 915 - (vmin_lim - vmin) * 1000 - 100;
    if (charger_duty < 150)
    {
      charger_duty = 150;
    }
  }

  if (vmin >= vmin_lim && vmax <= vmax_lim_low)
  {
    charger_duty = 915;
  }

  if (vmin >= vmin_lim && vmax >= vmax_lim_low && vmax <= vmax_lim_upp)
  {
    charger_duty = (vmax_lim_upp - vmax) * 3800 + 150;
  }

  if (vmax > vmax_lim_upp)
  {
    charger_duty = 0;
  }
  /*
  if (temp <= celltemp_min)
  {
    if (celltemp_min - temp > tempdif_max)
    {
      charger_duty = 0;
    }
    else
    {
      charger_duty -= (celltemp_min - temp) * 10; // subtract the temp difference times 10
    }
  }

  if (temp >= celltemp_max) // check if temp is outside of preferred range but still within max deviation
  {
    if (temp - celltemp_max > tempdif_max)
    {
      charger_duty = 0;
    }
    else
    {
      charger_duty -= (temp - celltemp_max) * 10;
    }
  }
  */
  if ((charger_duty > 0 && charger_duty < 100) || charger_duty < 0)
  {
    charger_duty = 0;
  }

  if (balcap > balcap_max)
  {
    charger_duty = 0;
  }
}

void LockEvse(bool)
{
  if (true)
  {
    analogWrite(unlock_plug_P, 255); // switch P fet unlock off
    analogWrite(unlock_plug_N, 0);   // switch N fet unlock off
    delayMicroseconds(1);            // wait for fet delay & fall time
    analogWrite(lock_plug_P, 0);     // switch P fet lock on
    analogWrite(lock_plug_N, 255);   // switch N fet lock on
    delayMicroseconds(1);
  }
  if (false)
  {
    analogWrite(lock_plug_P, 255);   // turn P fet lock off
    analogWrite(lock_plug_N, 0);     // turn N fet lock off
    delayMicroseconds(1);            // wait for fet delay & fall time
    analogWrite(unlock_plug_P, 0);   // turn P fet unlock on
    analogWrite(unlock_plug_N, 255); // turn N fet unlock on
  }
}

#ifdef TARGET_ESP32

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

  String chrgrspeed_json = "{\"charger_speed\":";
  chrgrspeed_json += charger_speed;
  chrgrspeed_json += "}";

  String celltemp_json = "{\"celltemp\":";
  celltemp_json += celltemp;
  celltemp_json += "}";
}

String processor(const String &var)
{
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER")
  {
    String buttons = "";
    String outputStateValue = "hello";
    buttons += "<label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label>";
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
    request->send(page, "/datalog", "text/html", false);
  });
  server.on("/logfile", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/log/logfile.txt", "r");
    request->send(page, "/logfile", "text/plain", false);
  });
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/html/test.html", "r");
    request->send(page, "/test", "text/html", false, processor);
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/css/style.css", "r");
    request->send(page, "/style.css", "text/css");
  });
  //server.serveStatic("/favicon.ico", SD, "/etc/favicon.ico");
  server.on("/pureknob.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/js/pureknob.js", "r");
    request->send(page, "/pureknob.js", "text/javascript");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/etc/favicon.ico", "r");
    request->send(page, "/favicon.ico", "image/vnd.microsoft.icon");
  });
  server.on(
      "/update/#0", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
      },
      handleUpload);
  server.on(
      "/update/#1", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200);
      },
      handleUploadWebpage);
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
/*
void handleUploadWebpage(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final)
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
  Serial.println("starting upload!");
  if (!index)
  {
    Serial.println(filename);
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