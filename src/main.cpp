// headers
#include <credentials.h>
#include <web_server.h>
#include <web_updater.h>
#include <charger.h>
#include <globals.h>

void LockEvse(bool lock_evse);

/* Timers */
elapsedMillis since_int1 = 0;
elapsedMillis since_int2 = 0;
const long int1 = 10000;  //bms request interval
const long int2 = 120000; // every 2 minutes
int counter;
long time_minutes = 0;

typedef int32_t esp_err_t;
//#define Serial1 Serial

File logfile;
int logfile_nr = EEPROM.readInt(0);

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
  if (GetSerialData() == "Succes")
  {
    ControlCharger();
  } else
  {
    ControlCharger(false); //turn of charger
  }
}

void loop()
{
  ArduinoOTA.handle();

  time_minutes = millis() / 60000; // time in minutes

  evse_on = gpio_get_level(EVSE);
  evse_on = !evse_on;
  charge_limited = gpio_get_level(CHARGER_LIMITED);

  if (since_int1 > int1)
  {
    Serial.println("\n\nGetting Data");
    if (GetSerialData() == "Succes")
    {
      ControlCharger();
    }
    else
    {
      ControlCharger(false);
    }
    if (endofcharge == true && is_balancing == false);
    {
      esp_light_sleep_start();
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

void LockEvse(bool state)
{
  if (state == true)
  {
    ledcWrite(unlock_high, 200); // switch P fet unlock off
    ledcWrite(unlock_low, 0);    // switch N fet unlock off
    delayMicroseconds(1);        // wait for fet delay & fall time
    ledcWrite(lock_high, 0);     // switch P fet lock on
    ledcWrite(lock_low, 200);    // switch N fet lock on
    delayMicroseconds(1);
  }
  if (state == false)
  {
    ledcWrite(lock_high, 200);  // turn P fet lock off
    ledcWrite(lock_low, 0);     // turn N fet lock off
    delayMicroseconds(1);       // wait for fet delay & fall time
    ledcWrite(unlock_high, 0);  // turn P fet unlock on
    ledcWrite(unlock_low, 200); // turn N fet unlock on
  }
}