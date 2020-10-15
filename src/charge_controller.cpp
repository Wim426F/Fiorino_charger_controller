#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <AsyncWebSocket.h>
#include <AsyncJson.h>
#include <SD.h>
#include <SPI.h>
#include <elapsedMillis.h>
#include <string.h>
//#include "esp32_bt_music_receiver.h"

void StartMdnsService();
void StartWebServer();
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
//void handleUploadWebpage(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUploadWebpage(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void SendSocketData();
String GetSerialData(String input = "");
void ControlCharger();
void LockEvse(bool lock_evse);

/* Timers */
elapsedMillis since_int1 = 0;
elapsedMillis since_int2 = 0;
const long int1 = 20000; //bms request interval
const long int2 = 5000;
int rx_timeout = 0;
int counter;

/* Variables */
String serial_str;
String celltemp_str;
String vmin_str;
String vmax_str;
String balcap_str;

int celltemp_idx = 0;
int vmin_idx = 0;
int vmax_idx = 0;
int balcap_idx = 0;
int bms_state = 0; // 0:not charging   1:charging   2:charging ready

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

/* Triggers */
bool endofcharge = false;
bool evse_on = false;
bool soclim = false;

/* Connectivity */
const char *sta_ssid = "Boone-Huis";
const char *sta_password = "b00n3meubelstof@";
const char *ap_ssid = "Fiorino";
const char *ap_password = "3VLS042020";
const char *ota_hostname = "FIORINO_ESP32";
const char *ota_password = "MaPe1!";
const char *dns_hostname = "fiorino";
const char *http_username = "wim";
const char *http_password = "w1mb00n3";
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

/* GPIO */
#define chargerlim_pin GPIO_NUM_36
#define EVSE GPIO_NUM_39
#define RX1 GPIO_NUM_35
#define TX1 GPIO_NUM_33
#define BCK GPIO_NUM_13
#define LCK GPIO_NUM_15
#define DIN GPIO_NUM_14

/* PWM channels */
const uint8_t chargerpwm_ch = 1;
const uint8_t lock_low = 2; 
const uint8_t lock_high = 3; 
const uint8_t unlock_low = 4;
const uint8_t unlock_high = 5;

/* // Bluetooth 
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
      .bck_io_num = BCK, 
      .ws_io_num = LCK, 
      .data_out_num = DIN, 
      .data_in_num = I2S_PIN_NO_CHANGE};
  
  a2d_sink.set_i2s_config(i2s_config);
  //a2d_sink.set_pin_config(i2s_pin_config);
  a2d_sink.start(blde_name);
}
*/

void setup()
{
  Serial.begin(115200);
  Serial1.setRxBufferSize(4096);
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);
  serial_str.reserve(4096);
  SPI.begin(18, 19, 23, 5);
  SD.begin(GPIO_NUM_5, SPI, 80000000);
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(local_ip, gateway, subnet);
  WiFi.begin(sta_ssid, sta_password);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password, ssid_hidden, max_connection);
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wifi connected to: " + (String)sta_ssid);
  }
  Serial.println(WiFi.localIP());
  //Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();
  StartMdnsService();
  StartWebServer();
  //BluetoothAudio();

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO);
  esp_sleep_enable_uart_wakeup(0);
  esp_sleep_enable_uart_wakeup(1);
  esp_err_t gpio_set_pull_mode(gpio_num_t EVSE, gpio_pull_mode_t GPIO_PULLDOWN_ONLY);
  esp_err_t gpio_set_pull_mode(gpio_num_t chargerlim_pin, gpio_pull_mode_t GPIO_PULLDOWN_ONLY);
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
}

void loop()
{
  ArduinoOTA.handle();
  if (WiFi.status() != WL_CONNECTED)
  {
    available_networks = WiFi.scanNetworks(sta_ssid, sta_ssid);
    if (available_networks == 1)
    {
      WiFi.begin(sta_ssid, sta_password);
    }
  }
  if (!SD.exists("/html/index.html"))
  {
    Serial.println("SD card removed!");
    SD.end();
    SD.begin(5, SPI, 80000000);
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
  evse_on = gpio_get_level(EVSE);
  evse_on = !evse_on;
  soclim = gpio_get_level(chargerlim_pin);

  if (since_int1 > int1)
  {
    since_int1 = since_int1 - int1;
    GetSerialData("t");  
  }

  if (GetSerialData() == "Succes")
  {
    ControlCharger();
  } 

  if (since_int2 > int2)
  {
    since_int2 = since_int2 - int2;
    Serial.println((String) "vmin: " + vmin + "   vmax: " + vmax + "   PWM: " + charger_duty);
  }

  // if charging has finished put esp32 in light sleep
  if (vmax > 4.20 && GetSerialData() == "Failed")
  {
    charger_duty = 0;
    endofcharge = true;
    esp_light_sleep_start();
  }
  if (evse_on == true)
  {
    LockEvse(true);
  }
  else
  {
    LockEvse(false);
  }
}

String GetSerialData(String input)
{
  File logfile = SD.open("/log/logfile.txt", FILE_APPEND);
  serial_str = "";
  if (input.length() < 2)
  {
    input += "\r";
    Serial1.print(input);
    Serial.print(input);
  }
  while (!Serial1.available() && rx_timeout <= 10000)
  {
    rx_timeout++;
    delay(1);
  }
  if (rx_timeout > 10000)
  {
    Serial.println("No data received: timeout");
    rx_timeout = 0;
    logfile.println("-------------------------");
    logfile.println("No data received: timeout");
    logfile.println("-------------------------");
  }
  while (Serial1.available() > 0)
  {
    delayMicroseconds(1);
    serial_str += char(Serial1.read());
  }
  if (serial_str.length() > 50)
  {
    Serial.println("BMS data received!");
    Serial.println(serial_str);
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
    logfile.println((String) "vmin: " + vmin + "   vmax: " + vmax + "   PWM: " + charger_duty);
    logfile.close();
  } else
  {
    Serial.println("Serial data corrupt");
    Serial.println(serial_str);
  }
  
  if (bms_state > 1)
  {
    return "Failed";
  }
  else
  {
    return "Succes";
  }
}

void ControlCharger()
{
  if (vmin < vmin_lim)
  {
    charger_duty = 910 - (vmin_lim - vmin) * 1000 - 150;
    if (charger_duty < 100)
    {
      charger_duty = 150;
    }
  }

  if (vmin >= vmin_lim && vmax <= vmax_lim_low)
  {
    charger_duty = 910;
  }

  if (vmin >= vmin_lim && vmax >= vmax_lim_low && vmax <= vmax_lim_upp)
  {
    charger_duty = (vmax_lim_upp - vmax) * 3800 + 100;
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
  ledcWrite(chargerpwm_ch, charger_duty);
}

void LockEvse(bool)
{
  if (true)
  {
    ledcWrite(unlock_high, 200); // switch P fet unlock off
    ledcWrite(unlock_low, 0);   // switch N fet unlock off
    delayMicroseconds(1);          // wait for fet delay & fall time
    ledcWrite(lock_high, 0);     // switch P fet lock on
    ledcWrite(lock_low, 200);   // switch N fet lock on
    delayMicroseconds(1);
  }
  if (false)
  {
    ledcWrite(lock_high, 200);   // turn P fet lock off
    ledcWrite(lock_low, 0);     // turn N fet lock off
    delayMicroseconds(1);          // wait for fet delay & fall time
    ledcWrite(unlock_high, 0);   // turn P fet unlock on
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

  String chrgrspeed_json = "{\"charger_speed\":";
  chrgrspeed_json += charger_speed;
  chrgrspeed_json += "}";

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
    //buttons += "<label class=\"button\"><input value=\"Download\" type=\"button\" oncick=\"editlog(this)\" id=\"download\" " + outputState(2) + "></label>";
    //buttons += "<label class=\"button\"><input value=\"Delete\" type=\"button\" onclick=\"editlog(this)\" id=\"delete\" " + outputState(4) + "></label>";
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
    bool download = false;
    request->send(page, "/datalog", "text/html", download, processor);
    String logfunctions;
    // GET input1 value on fiorino.local/datalog?function=<download>
    if (request->hasParam(PARAM_INPUT_1))
    {
      logfunctions = request->getParam(PARAM_INPUT_1)->value();
    }
    if (logfunctions == "delete")
    {
      page.close();
      logfile = SD.open("/log/logfile.txt", FILE_WRITE);
      logfile.print("");
      logfile.close();
    }
    if (logfunctions == "download")
    {
      download = true;
    }
  });
  server.on("/logfile", HTTP_GET, [](AsyncWebServerRequest *request) {
    File page = SD.open("/log/logfile.txt", "r");
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
