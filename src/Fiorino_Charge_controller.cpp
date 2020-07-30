#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <LittleFS.h>

void ChargerControl();
void EvseActuator();
void WifiAutoConnect();
void ServeWebPage();

long previous_millis = 0;
unsigned long delay_one = 10000;
unsigned long delay_two = 100;

/* Data */
String data_string;
String temperature_string;
String Vmin_string;
String Vmax_string;
String Vtot_string;
String energy_dissipated_string;

int temperature_index = 0;
int Vmin_index = 0;
int Vmax_index = 0;
int Vtot_index = 0;
int energy_dissipated_index = 0;

float Vmin = 0;
float Vmax = 0;
float Vtot = 0;
float temperature = 0;
float charger_pwm_duty = 0;
float charger_speed = 0;

/* Limits */
const float temperature_min = 0.0;
const float temperature_max = 40.0;
const float temperature_dif_max = 10.0;
const float Vmin_lim = 3.000;
const float Vmax_lim_low = 4.000;
const float Vmax_lim_med = 4.100;
const float Vmax_lim_max = 4.200;
const float Vtot_lim = 300.000;
const float energy_dissipated_max = 2000.00;

/* Triggers */
boolean plug_is_locked = false;
boolean evse_is_on = false;
boolean voltage_is_limited = false;

/* Inputs */
const byte evse_pin = D5;
const byte charger_lim_pin = D6;

/* Outputs */
const byte charger_pwm_pin = D0;
const byte lock_plug_N = D1; // fet low side
const byte lock_plug_P = D2; // fet high side
const byte unlock_plug_N = D3;
const byte unlock_plug_P = D4;

/* Wifi */
const char *sta_ssid_one = "BooneZaak";
const char *sta_password_one = "B00n3meubelstof@";
const char *sta_ssid_two = "Boone-Huis";
const char *sta_password_two = "b00n3meubelstof@";
const char *ap_ssid = "Fiorino";
const char *ap_password = "3VLS042020";
const char *ota_hostname = "FIORINO_ESP6266";
const char *ota_password = "MaPe1!";
byte wifi_available = 0;

int ssid_hidden = 0;
int max_connection = 2;
IPAddress local_ip(192, 168, 1, 173);
IPAddress ap_ip(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server;
MDNSResponder mdns;
FS* filesystem = &LittleFS;

void setup()
{
  // Soft Acces Point and Station
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(sta_ssid_two, sta_password_two);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
  }
  //WifiAutoConnect();
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password, ssid_hidden, max_connection);

  // Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.onStart([]() {
    Serial.println("Start update");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up webserver
  mdns.begin("my-fiorino", WiFi.localIP());
  LittleFS.begin();
  server.on("/", ServeWebPage);
  server.begin();
  MDNS.addService("http", "tcp", 80);

  Serial.begin(115200);
  while (!Serial)
  {
    delay(50);
  }

  pinMode(evse_pin, INPUT_PULLUP);
  pinMode(charger_lim_pin, INPUT_PULLUP);

  pinMode(charger_pwm_pin, OUTPUT);
  pinMode(lock_plug_N, OUTPUT);
  pinMode(lock_plug_P, OUTPUT);
  pinMode(unlock_plug_N, OUTPUT);
  pinMode(unlock_plug_P, OUTPUT);

  data_string.reserve(3700);
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();

  if (WiFi.status() != WL_CONNECTED)
  {
    wifi_available = WiFi.scanNetworks(sta_ssid_one, sta_ssid_two);
    if (wifi_available > 0)
    {
      WifiAutoConnect();
    }
  }

  EvseActuator();

  evse_is_on = digitalRead(evse_pin);
  evse_is_on = !evse_is_on;

  voltage_is_limited = digitalRead(charger_lim_pin);
  voltage_is_limited = !voltage_is_limited;

  unsigned long current_millis = millis();

  if (current_millis - previous_millis >= delay_one && evse_is_on == true)
  {
    Serial.write('T'); // retrieve data
  }

  if (current_millis - previous_millis >= delay_two)
  {
    Serial.println(WiFi.localIP());
    Serial.println(WiFi.dnsIP());
    Serial.println(WiFi.gatewayIP());
    Serial.println(WiFi.subnetMask());
    //Serial.println((String) "Vmin: " + Vmin + "   Vmax: " + Vmax + "   Vtot: " + Vtot + "   Temp: " + temperature + "   PWM: " + charger_pwm_duty);
    previous_millis = current_millis;
  }

  //while (Serial.available() > 0 && plug_in == true)
  while (Serial.available() > 0)
  {
    data_string = Serial.readString();
    data_string.replace(" ", "");
    data_string.replace("N.C.", "");
    data_string.replace("*", "");
    temperature_index = data_string.indexOf("d2-7f") + 26; // find the location of certain strings
    data_string.remove(0, temperature_index);              // remove everything until temp to free up memory
    Vmin_index = data_string.indexOf("Vmin:") + 5;
    Vmax_index = data_string.indexOf("Vmax:") + 5;
    Vtot_index = data_string.indexOf("Vtot:") + 5;

    temperature_string = data_string.substring(0, 6);
    Vmin_string = data_string.substring(Vmin_index, Vmin_index + 5);
    Vmax_string = data_string.substring(Vmax_index, Vmax_index + 5);
    Vtot_string = data_string.substring(Vtot_index, Vtot_index + 5);

    if (temperature_string.startsWith(String('-')))
    {
      temperature = temperature_string.toFloat();
    }
    else
    {
      temperature_string.remove(0, 1);
      temperature_string.replace(String('S'), "");
      temperature = temperature_string.toFloat();
    }

    Vmin = Vmin_string.toFloat(); // convert string to float
    Vmax = Vmax_string.toFloat();
    Vtot = Vtot_string.toFloat();
    previous_millis = current_millis;
    ChargerControl();
  }

  analogWrite(charger_pwm_pin, charger_pwm_duty);
}

void ChargerControl()
{
  if (Vmin < Vmin_lim)
  {
    charger_pwm_duty = 229 - (Vmin_lim - Vmin) * 200;
    if (charger_pwm_duty < 26)
    {
      charger_pwm_duty = 26;
    }
  }

  if (Vmin >= Vmin_lim && Vmax <= Vmax_lim_low)
  {
    charger_pwm_duty = 229;
  }

  if (Vmin >= Vmin_lim && Vmax > Vmax_lim_low && Vmax <= Vmax_lim_med && voltage_is_limited == true)
  {
    charger_pwm_duty = (Vmax_lim_med - Vmax) * 2040 + 25; // PWM is from 26 > 229 (10% > 90%)
  }

  if (Vmin >= Vmin_lim && Vmax >= Vmax_lim_low && Vmax <= Vmax_lim_max && voltage_is_limited == false)
  {
    charger_pwm_duty = (Vmax_lim_max - Vmax) * 1020 + 25;
  }

  if ((Vmax > Vmax_lim_max || Vtot > Vtot_lim) || (Vmax > Vmax_lim_med && voltage_is_limited == true))
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

  if ((charger_pwm_duty > 0 && charger_pwm_duty < 26) || charger_pwm_duty < 0)
  {
    charger_pwm_duty = 0;
  }

  charger_speed = (map(charger_pwm_duty, 26, 229, 0, 27)) * Vtot / 1000;

  return;
}

void EvseActuator()
{
  if (evse_is_on == true && plug_is_locked == false) // if the evse is plugged in and not latched
  {
    analogWrite(unlock_plug_P, 255); // switch P fet unlock off
    analogWrite(unlock_plug_N, 0);   // switch N fet unlock off
    delayMicroseconds(1);            // wait for fet delay & fall time
    analogWrite(lock_plug_P, 0);     // switch P fet lock on
    analogWrite(lock_plug_N, 255);   // switch N fet lock on
    delayMicroseconds(1);
    plug_is_locked = true;
  }

  if (evse_is_on == false && plug_is_locked == true) // if the evse is stopped
  {
    analogWrite(lock_plug_P, 255);   // turn P fet lock off
    analogWrite(lock_plug_N, 0);     // turn N fet lock off
    delayMicroseconds(1);            // wait for fet delay & fall time
    analogWrite(unlock_plug_P, 0);   // turn P fet unlock on
    analogWrite(unlock_plug_N, 255); // turn N fet unlock on
    plug_is_locked = false;
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
  if (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED)
  {
    wifi_available = 0;
  }
}

void ServeWebPage()
{
  File file = LittleFS.open("/webpage.html", "r"); // read webpage from filesystem
  server.streamFile(file, "text/html");
  file.close();
}
