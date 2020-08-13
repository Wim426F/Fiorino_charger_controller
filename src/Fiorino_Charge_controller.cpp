#include <Arduino.h>
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

void WifiAutoConnect();
void handleRoot();
void handleJS();
void handleFileList();
void GetSerialData();
void ChargerControl();
void EvseLocked();
void getData();
//void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

unsigned long prev_millis_one = 0;
unsigned long prev_millis_two = 0;
unsigned long prev_millis_three = 0;
const unsigned long charger_timeout = 18000000; // 5 hours
const long delay_one = 30000;
const long delay_two = 5000;

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
const float Vmax_lim_med = 4.100;
const float Vmax_lim_max = 4.200;
const float dissipated_nrg_max = 2000.00;

int rx_timeout;

/* Triggers */
boolean plug_locked = false;
boolean evse_on = false;
boolean voltage_lim = false;
boolean bms_get_stat = true;


/* Inputs */
const byte evse_pin = D5;
const byte charger_lim_pin = D6;

/* Outputs */
const byte charger_pwm_pin = D0;
const byte lock_plug_N = D1; // fet low side
const byte lock_plug_P = D2; // fet high side
const byte unlock_plug_N = D4;
const byte unlock_plug_P = D3;


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

void start_mdns_service()
{
    //initialize mDNS service
    esp_err_t err = mdns_init();
    if (err) {
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

void setup()
{
  serial_str.reserve(5000);
  SD.begin(8);
  WiFi.mode(WIFI_AP_STA);
  WiFi.config(local_ip, gateway, subnet);
  WiFi.begin(sta_ssid_one, sta_password_one);

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
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password, ssid_hidden, max_connection);

  // Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();

  // Set up webserver
  start_mdns_service();
  server.on("/", handleRoot);
  //server.addHandler(new handleJS("/pureknob.js"));
  server.on("/pureknob.js", handleJS);
  server.on("/list", handleFileList);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  analogWriteResolution(10);
  analogWriteFrequency(1000);
  analogWrite(charger_pwm_pin, 30);
  Serial.begin(115200);
  while (!Serial)
  {
    delay(10);
  }
  Serial1.begin(115200);
  while (!Serial)
  {
    delay(10);
  }

  pinMode(evse_pin, INPUT_PULLUP);
  pinMode(charger_lim_pin, INPUT_PULLUP);

  pinMode(charger_pwm_pin, OUTPUT);
  pinMode(lock_plug_N, OUTPUT);
  pinMode(lock_plug_P, OUTPUT);
  pinMode(unlock_plug_N, OUTPUT);
  pinMode(unlock_plug_P, OUTPUT);
  delay(3000);
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  webSocket.loop();

  available_networks = WiFi.scanNetworks(sta_ssid_one, sta_ssid_two);
  if (WiFi.status() != WL_CONNECTED && available_networks > 0)
  {
    WifiAutoConnect();
  }

  unsigned long current_millis = millis();
  evse_on = digitalRead(evse_pin);
  //evse_on = !evse_on;
  voltage_lim = digitalRead(charger_lim_pin);
  voltage_lim = !voltage_lim;

  if (current_millis - prev_millis_one >= delay_one)
  {
    Serial1.println("t");
    Serial.println("t");

    while (Serial1.available() == 0 && rx_timeout <= 8)
    {
      rx_timeout++;
      delay(1000);
      Serial.println("waiting for incoming data");
    }
    if (rx_timeout >= 5)
    {
      Serial.println("No data received: timeout");
      rx_timeout = 0;
    }

    while (Serial1.available() > 0)
    {
      delay(1); //delay to allow byte to arrive in input buffer
      char c = Serial1.read();
      serial_str += c;
    }
    if (serial_str.length() > 0)
    {
      GetSerialData();
      Serial.println("BMS data received!");
      ChargerControl();
      serial_str = "";
    }
    prev_millis_one = current_millis;
  }

  if (current_millis - prev_millis_two >= delay_two)
  {

    //Serial.println(serial_str);
    Serial.println((String) "Vmin: " + Vmin + "   Vmax: " + Vmax + "   Temp: " + temperature + "   PWM: " + charger_pwm_duty);
    prev_millis_two = current_millis;
  }

  if (plug_locked == false)
  {
    EvseLocked();
  }

  if (evse_on == true)
  {
    analogWrite(charger_pwm_pin, charger_pwm_duty);
  }
}

void GetSerialData()
{
  serial_str.replace(" ", "");
  serial_str.replace("*", "");
  serial_str.replace("N.C.", "");

  if (bms_get_stat == true)
  {
    temperature_idx = serial_str.indexOf("SOC") - 5; // find the location of certain strings
    //data_string.remove(0, temperature_idx);
    delay(5);
    Vmin_idx = serial_str.indexOf("Vmin:") + 5;
    Vmax_idx = serial_str.indexOf("Vmax:") + 5;

    temperature_str = serial_str.substring(temperature_idx, temperature_idx + 5);
    Vmin_str = serial_str.substring(Vmin_idx, Vmin_idx + 5);
    Vmax_str = serial_str.substring(Vmax_idx, Vmax_idx + 5);

    if (temperature_str.startsWith(String('-')))
    {
      temperature = temperature_str.toFloat();
    }
    else
    {
      temperature_str.remove(0, 1);
      temperature_str.replace(String('S'), "");
      temperature = temperature_str.toFloat();
    }

    Vmin = Vmin_str.toFloat(); // convert string to float
    Vmax = Vmax_str.toFloat();
    bms_get_stat = true;
  }
  else // bms get balance current
  {
    int check_type = 0;
    dissipated_nrg_idx = serial_str.indexOf("dissipata") + 10;
    serial_str.remove(0, dissipated_nrg_idx);
    for (check_type = 0; isDigit(serial_str.charAt(check_type) == true); check_type++)
    {
    }
    dissipated_nrg_str = serial_str.substring(0, check_type);
    dissipated_nrg = dissipated_nrg_str.toFloat();
    bms_get_stat = true;
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

  if (Vmin >= Vmin_lim && Vmax > Vmax_lim_low && Vmax <= Vmax_lim_med && voltage_lim == true)
  {
    charger_pwm_duty = (Vmax_lim_med - Vmax) * 4096 + 150; // PWM is from 150 > 915 (15% > 90%)
  }

  if (Vmin >= Vmin_lim && Vmax >= Vmax_lim_low && Vmax <= Vmax_lim_max && voltage_lim == false)
  {
    charger_pwm_duty = (Vmax_lim_max - Vmax) * 3800 + 150;
  }

  if (Vmax > Vmax_lim_max || (Vmax > Vmax_lim_med && voltage_lim == true))
  {
    charger_pwm_duty = 0;
  }

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

  if ((charger_pwm_duty > 0 && charger_pwm_duty < 100) || charger_pwm_duty < 0)
  {
    charger_pwm_duty = 0;
  }

  if (dissipated_nrg > dissipated_nrg_max)
  {
    charger_pwm_duty = 0;
  }

  charger_speed = map(charger_pwm_duty, 100, 915, 0, 100);
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


