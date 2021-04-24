// headers
#include <credentials.h>
#include <web_server.h>
#include <web_updater.h>
#include <charger.h>
#include <globals.h>

using namespace std;

/* Timers */
elapsedSeconds since_int1 = 0;
elapsedSeconds since_int2 = 0;
elapsedSeconds since_start = 0;
long int1 = 2;   // bms request interval
long int2 = 240; // 2 min, datalogger interval
int counter;
int time_minutes = 0;

typedef int32_t esp_err_t;
//#define Serial1 Serial

File logfile;
int logfile_nr = EEPROM.readInt(0);

/* GPIO */
#define CHARGER_LIMITED GPIO_NUM_36
#define EVSE GPIO_NUM_39
#define RX1 GPIO_NUM_35
#define TX1 GPIO_NUM_33
#define PWM GPIO_NUM_26

/* PWM channels */
const uint8_t chargerpwm_ch = 1;
const uint8_t lock_evse = 3;
const uint8_t unlock_evse = 5;

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RX1, TX1);
  Serial1.setRxBufferSize(4096);
  SD.begin(SS, SPI, 80000000, "/sd", 20);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, password);

  //Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.begin();
  StartMdnsService();
  StartWebServer();

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO);
  esp_sleep_enable_uart_wakeup(0);
  esp_sleep_enable_uart_wakeup(1);
  esp_err_t gpio_set_pull_mode(gpio_num_t EVSE, gpio_pull_mode_t GPIO_PULLDOWN_ONLY);
  esp_err_t gpio_set_pull_mode(gpio_num_t CHARGER_LIMITED, gpio_pull_mode_t GPIO_PULLDOWN_ONLY);

  ledcSetup(chargerpwm_ch, 1000, 10); // channel, freq, res
  ledcSetup(lock_evse, 1000, 8);
  ledcSetup(unlock_evse, 1000, 8);

  ledcAttachPin(PWM, chargerpwm_ch);
  ledcAttachPin(GPIO_NUM_21, lock_evse);
  ledcAttachPin(GPIO_NUM_16, unlock_evse);

  delay(500);
  
  if (ParseStringData() == "Succes")
  {
    ControlCharger();
  } 
  dataLogger("start"); 
}

void loop()
{
  ArduinoOTA.handle();

  time_minutes = since_start / 60; // time in minutes

  str_vmin = (String)vmin;
  str_vmax = (String)vmax;
  str_vtot = (String)vtot;
  str_ctmp = (String)celltemp;
  str_soc = (String)stateofcharge;
  str_lem = (String)lemsensor;

  evse_on = gpio_get_level(EVSE);
  evse_on = !evse_on;
  charge_limited = gpio_get_level(CHARGER_LIMITED);

  if (since_int1 > int1)
  {
    Serial.println("\n\nGetting Data");
    string status = ParseStringData();

    if (status == "Succes")
    {
      ControlCharger();
    }
    if (status == "Fail")
    {
      ControlCharger(false);
    }

    if (endofcharge == true)
    {
      dataLogger("finished");
    } 
    if (trickle_phase == true)
    {
      int1 = 20; // too frequent refreshing data during trickle phase doesn't work well
    }

    since_int1 -= int1;
  }

  if (since_int2 > int2)
  {
    dataLogger();
    since_int2 -= int2;
  }

  if (evse_on == true)
  {
    // H-bridge 
    ledcWrite(unlock_evse, 200); 
    ledcWrite(lock_evse, 0);    
  }
  else
  {
    // Reverse h-bridge
    ledcWrite(lock_evse, 200);  
    ledcWrite(unlock_evse, 0);  
  } 
}