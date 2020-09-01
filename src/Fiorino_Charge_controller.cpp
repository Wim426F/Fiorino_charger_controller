#include <Arduino.h>
#include <elapsedMillis.h>
#ifdef TARGET_ESP32
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <analogWrite.h>
#include "esp32_bt_music_receiver.h"
#endif

byte buffer[50*1024];

void WifiAutoConnect();
void handleRoot();
void handleJS();
void handleFileList();
bool GetSerialData();
void ChargerControl();
void EvseLocked();
void getData();
//void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

elapsedMillis since_interval1 = 0;
elapsedMillis since_interval2 = 0;
unsigned long prev_millis_three = 0;
const unsigned long charger_timeout = 18000000; // 5 hours
const long interval1 = 2000;                    //1 minute
const long interval2 = 2000;

/* Data */
String serial_str;
String temperature_str;
String Vmin_str;
String Vmax_str;
String dissipated_nrg_str;

int temperature_idx = 0;
int Vmin_idx = 0;
int Vmax_idx = 0;
int dissipated_nrg_idx = 0;
uint8_t bms_state = 0; // 0:not charging   1:charging   2:charging ready

float Vmin = 0;
float Vmax = 0;
float temperature = 0;
float charger_pwm_duty = 0;
float charger_speed = 0;
float dissipated_nrg = 0;

/* Limits */
const float temperature_min = 0.0;
const float temperature_max = 40.0;
const float temperature_dif_max = 10.0;
const float Vmin_lim = 3.000;
const float Vmax_lim_low = 4.000;
const float Vmax_lim_max = 4.200;
const float dissipated_nrg_max = 2000.00;
int rx_timeout;

/* Triggers */
bool plug_locked = false;
bool evse_on = false;
bool voltage_lim = false;
bool bms_get_stat = true;

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

#ifdef TARGET_ESP32
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

/* Bluetooth */
BluetoothA2DSink a2d_sink;
char *blde_name = "FIORINO_BT";

/* Wifi */
const char *sta_ssid_one = "Boone-Huis";
const char *sta_password_one = "b00n3meubelstof@";
const char *sta_ssid_two = "BooneZaak";
const char *sta_password_two = "B00n3meubelstof@";
const char *ap_ssid = "Fiorino";
const char *ap_password = "3VLS042020";
const char *ota_hostname = "FIORINO_ESP8266";
const char *ota_password = "MaPe1!";
int ssid_hidden = 0;
int max_connection = 3;
int available_networks = 0;
int wifi_timeout = 0;
IPAddress local_ip(192, 168, 1, 173);
IPAddress ap_ip(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);
WebServer server(80);
MDNSResponder mdns;
WebSocketsServer webSocket = WebSocketsServer(81);

void bluetoothAudio()
{
  static const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100,                         // corrected by info from bluetooth
      .bits_per_sample = (i2s_bits_per_sample_t)16, /* the DAC module will only take the 8bits from MSB */
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

void startMdnsService()
{

  //initialize mDNS service
  esp_err_t err = mdns_init();
  if (err)
  {
    printf("MDNS Init failed: %d\n", err);
    return;
  }
  //set hostname
  mdns_hostname_set("myfiorino");
  //set default instance
  mdns_instance_name_set("Fiorino");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  // Do something with the data from the client
}
#endif

void setup()
{
#ifdef TARGET_ESP32
  SD.begin(8);
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(local_ip, gateway, subnet);
  WiFi.begin(sta_ssid_one, sta_password_one);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password, ssid_hidden, max_connection);
  while (WiFi.status() != WL_CONNECTED && wifi_timeout < 30)
  {
    delay(100);
    wifi_timeout++;
    if (wifi_timeout >= 10)
    {
      wifi_timeout = 0;
      break;
    }
  }
  // Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();

  // Set up webserver
  startMdnsService();
  bluetoothAudio();
  server.on("/", handleRoot);
  //server.addHandler(new handleJS("/pureknob.js"));
  server.on("/pureknob.js", handleJS);
  server.on("/list", handleFileList);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  analogWriteResolution(10);
  analogWriteFrequency(1000);
  Serial.setRxBufferSize(4096);
  Serial1.setRxBufferSize(4096);
#endif
#ifdef TEENSY_40
  analogWriteFrequency(5, 1000);
  analogWriteResolution(10);
#endif
  Serial.begin(115200);
  Serial1.begin(115200);
  serial_str.reserve(5000);

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
  server.handleClient();
  webSocket.loop();

  available_networks = WiFi.scanNetworks(sta_ssid_one, sta_ssid_two);
  if (WiFi.status() != WL_CONNECTED && available_networks > 0)
  {
    WifiAutoConnect();
  }
#endif

  evse_on = digitalRead(evse_pin);
  //evse_on = !evse_on;
  voltage_lim = digitalRead(charger_lim_pin);
  voltage_lim = !voltage_lim;

  if (since_interval1 > interval1)
  {
    since_interval1 = since_interval1 - interval1;
    GetSerialData();
  }

  if (GetSerialData() == true)
  {
    ChargerControl();
  }

  if (since_interval2 > interval2)
  {
    since_interval2 = since_interval2 - interval2;
    Serial.println((String) "Vmin: " + Vmin + "   Vmax: " + Vmax + "   PWM: " + charger_pwm_duty);
  }
  //analogWrite(charger_pwm_pin, charger_pwm_duty);

  if (plug_locked == false)
  {
    EvseLocked();
  }

  if (evse_on == true)
  {
    analogWrite(charger_pwm_pin, charger_pwm_duty);
  }
}

bool GetSerialData()
{
  serial_str = "";
  Serial1.print('t');
  Serial1.print('\r');
  Serial.println("t");

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
    delay(1); //delay to allow byte to arrive in input buffer
    serial_str += char(Serial1.read());
  }
  if (serial_str.length() > 0)
  {
    Serial.println("BMS data received!");
    serial_str.replace(" ", "");
    serial_str.replace("*", "");
    serial_str.replace("N.C.", "");

    if (serial_str.indexOf("Vmin") > -1 && serial_str.indexOf("Vmax") > -1)
    {
      bms_state = 0;
      Vmin_idx = serial_str.indexOf("Vmin:") + 5;
      Vmax_idx = serial_str.indexOf("Vmax:") + 5;
    }

    if (serial_str.indexOf("Shunt") > -1) // Shunt is similar to Vmax
    {
      bms_state = 1;
      Vmin_idx = serial_str.indexOf("Vmed:") + 5;
      Vmax_idx = serial_str.indexOf("Shunt:") + 6;
    }

    temperature_idx = serial_str.indexOf("SOC") - 5; // find the location of certain strings
    temperature_str = serial_str.substring(temperature_idx, temperature_idx + 5);
    if (temperature_str.startsWith(String('-')))
    {
      temperature = temperature_str.toFloat();
    }
    else
    {
      temperature_str.remove(0, 1);
      temperature = temperature_str.toFloat();
    }

    Vmin_str = serial_str.substring(Vmin_idx, Vmin_idx + 5);
    Vmax_str = serial_str.substring(Vmax_idx, Vmax_idx + 5);
    Vmin = Vmin_str.toFloat(); // convert string to float
    Vmax = Vmax_str.toFloat();
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
  if (Vmin < Vmin_lim)
  {
    charger_pwm_duty = 915 - (Vmin_lim - Vmin) * 1000 - 100;
    if (charger_pwm_duty < 150)
    {
      charger_pwm_duty = 150;
    }
  }

  if (Vmin >= Vmin_lim && Vmax <= Vmax_lim_low)
  {
    charger_pwm_duty = 915;
  }

  if (Vmin >= Vmin_lim && Vmax >= Vmax_lim_low && Vmax <= Vmax_lim_max)
  {
    charger_pwm_duty = (Vmax_lim_max - Vmax) * 3800 + 150;
  }

  if (Vmax > Vmax_lim_max)
  {
    charger_pwm_duty = 0;
  }
  /*
  if (temperature <= temperature_min)
  {
    if (temperature_min - temperature > temperature_dif_max)
    {
      charger_pwm_duty = 0;
    }
    else
    {
      charger_pwm_duty -= (temperature_min - temperature) * 10; // subtract the temp difference times 10
    }
  }

  if (temperature >= temperature_max) // check if temperature is outside of preferred range but still within max deviation
  {
    if (temperature - temperature_max > temperature_dif_max)
    {
      charger_pwm_duty = 0;
    }
    else
    {
      charger_pwm_duty -= (temperature - temperature_max) * 10;
    }
  }
  */
  if ((charger_pwm_duty > 0 && charger_pwm_duty < 100) || charger_pwm_duty < 0)
  {
    charger_pwm_duty = 0;
  }

  if (dissipated_nrg > dissipated_nrg_max)
  {
    charger_pwm_duty = 0;
  }
}

void EvseLocked()
{
  if (evse_on == true && plug_locked == false) // if the evse is plugged in and not latched
  {
    analogWrite(unlock_plug_P, 255); // switch P fet unlock off
    analogWrite(unlock_plug_N, 0);   // switch N fet unlock off
    delayMicroseconds(1);            // wait for fet delay & fall time
    analogWrite(lock_plug_P, 0);     // switch P fet lock on
    analogWrite(lock_plug_N, 255);   // switch N fet lock on
    delayMicroseconds(1);
    plug_locked = true;
  }

  if (evse_on == false && plug_locked == true) // if the evse is stopped
  {
    analogWrite(lock_plug_P, 255);   // turn P fet lock off
    analogWrite(lock_plug_N, 0);     // turn N fet lock off
    delayMicroseconds(1);            // wait for fet delay & fall time
    analogWrite(unlock_plug_P, 0);   // turn P fet unlock on
    analogWrite(unlock_plug_N, 255); // turn N fet unlock on
    plug_locked = false;
  }
}

#ifdef TARGET_ESP32

void WifiAutoConnect()
{

  WiFi.begin(sta_ssid_one, sta_password_one);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
  }
  if (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED)
  {
    WiFi.begin(sta_ssid_two, sta_password_two);
  }
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
  }
}

void handleRoot()
{
  File file = SD.open("/index.xhtml", "r"); // read webpage from filesystem
  server.streamFile(file, "application/xhtml");
  file.close();
}

void handleJS()
{

  File file = SD.open("pureknob.js", "r");
  server.streamFile(file, "text/javascript");
  file.close();
}

void handleFileList()
{
  return;
}

void getData()
{
  String Vmin_json = "{\"vmin\":";
  Vmin_json += Vmin;
  Vmin_json += "}";
  webSocket.broadcastTXT(Vmin_json.c_str(), Vmin_json.length());

  String Vmax_json = "{\"vmax\":";
  Vmax_json += Vmax;
  Vmax_json += "}";
  webSocket.broadcastTXT(Vmax_json.c_str(), Vmax_json.length());
}

#endif