// headers
#include <globals.h>
#include <web_server.h>
#include <charger.h>
#include <uart_parser.h>
#include <can_parser.h>

using namespace std;

/* Timers */
elapsedSeconds since_start = 0;
elapsedMillis since_int1 = 0;
elapsedSeconds since_int2 = 0;

long int1 = 30;  // 30 millis interval adc and pwm sampling for evse
long int2 = 600; // 10 min datalogging interval
int time_minutes = 0;

typedef int32_t esp_err_t;

File logfile;
int logfile_nr = EEPROM.readInt(0);

uint8_t input_s1 = LOW;

float celltemp = 0;
float stateofcharge = 0;
float dc_amps = 0;
float vmin = 0;
float vmax = 0;
float vtot = 0;
float balancing_power = 0;
float balanced_capacity = 0;

String str_vmin;
String str_vmax;
String str_vtot;
String str_ctmp;
String str_soc;
String str_dc_amps;

bool car_is_off = false;

/* PWM channels */
const uint8_t chargerpwm_ch = 1;

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, UART_RX1, UART_TX1);
  Serial1.setRxBufferSize(4096);
  SD.begin(SD_CS, SPI, 80000000, "/sd", 20);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, password);

  //Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.begin();
  StartMdnsService();
  ConfigWebServer();

  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO);
  esp_sleep_enable_uart_wakeup(0);
  esp_sleep_enable_uart_wakeup(1);
  esp_sleep_enable_ext0_wakeup(INPUT_S1, LOW);
  esp_sleep_enable_ext0_wakeup(EVSE_PILOT, HIGH);
  gpio_set_direction(EVSE_PILOT, GPIO_MODE_INPUT);
  gpio_set_direction(EVSE_STATE_C, GPIO_MODE_OUTPUT);
  gpio_set_direction(S1_LED_GREEN, GPIO_MODE_OUTPUT);
  gpio_set_direction(S2_LED_RED, GPIO_MODE_OUTPUT);
  gpio_set_direction(HB_IN1, GPIO_MODE_OUTPUT);
  gpio_set_direction(HB_IN2, GPIO_MODE_OUTPUT);

  ledcSetup(chargerpwm_ch, 1000, 10); // channel, freq, res
  ledcAttachPin(PWM, chargerpwm_ch);

  adcAttachPin(EVSE_PROX);
  analogReadResolution(10);

  attachInterrupt(EVSE_PILOT, risingIRQ, RISING); // for EVSE pwm capture

  CAN.setPins(CAN_RX, CAN_TX);
  //CAN.begin(125E3); // 125 kbps

  dataLogger("start");
  serial_string.reserve(3500);

  ledcWrite(chargerpwm_ch, 500);
}

void loop()
{
  ArduinoOTA.handle();

  time_minutes = since_start / 60; // time in minutes

  input_s1 = gpio_get_level(INPUT_S1);

  str_vmin = (String)vmin;
  str_vmax = (String)vmax;
  str_vtot = (String)vtot;
  str_ctmp = (String)celltemp;
  str_soc = (String)stateofcharge;
  str_dc_amps = (String)dc_amps;

  /*  -----  Updating data from BMS  -----  */
  static bool request_t = true;
  static string request_state;
  
  if (request_t) // switch between requesting 't' and 'd' when balancing
  {
    request_state = GetSerialData("t"); // get cell voltages from bms
    if (bms_is_balancing)
    {
      request_t = false;
    }
  }
  
  if (bms_is_balancing && request_t == false && request_state == "succes")
  {
    GetSerialData("d"); // get balancing data from bms
    request_t = true;
  }

  static int request_fails = 0;

  if (request_state == "succes")
  {
    ControlCharger();
    request_state = "";
    request_fails = 0;
  }
  if (request_state == "fail")
  {
    ControlCharger(false);
    request_state = "";
    request_fails++;
  }
  if (request_fails > 5)
  {
    car_is_off = true; // if request fails, the car has probably shutdown
  }

  /*  -----  Control charge port actuator  -----  */
  if (evse_pilot_pwm >= 7 && evse_pilot_mvolt > 2000) // only lock cable if connected to EVSE charging station
  {
    // H-bridge lock
    digitalWrite(HB_IN1, HIGH);
    digitalWrite(HB_IN2, LOW);
  }
  else
  {
    // H-bridge unlock
    digitalWrite(HB_IN1, LOW);
    digitalWrite(HB_IN2, HIGH);
  }

  /*  -----  EVSE  -----  */
  if (evse_prox_mvolt < 3000) // cable is plugged in
  {
    static long since_evse_started;

    if (evse_ready) // if evse started, enable state A for 500ms, then move on to state C
    {
      since_evse_started = millis();

      if (millis() - since_evse_started > 500)
      {
        evse_ready = false;
        digitalWrite(EVSE_STATE_C, HIGH);
      }
    }

    if (trickle_phase) // go back to state A if charging is almost finished
    {
      digitalWrite(EVSE_STATE_C, LOW);
      digitalWrite(S1_LED_GREEN, HIGH); // green led is on at ~100% battery soc
      digitalWrite(S2_LED_RED, LOW);
    }
    else
    {
      digitalWrite(S1_LED_GREEN, LOW); // Red led is on <100% battery soc
      digitalWrite(S2_LED_RED, HIGH);
    }
  }
  else // cable is unplugged
  {
    ControlCharger(false);                        // turn off charger
    evse_ready = true; // resets evse starting sequence for next session
    digitalWrite(S1_LED_GREEN, LOW);
    digitalWrite(S2_LED_RED, LOW);
  }

  /*  -----  Power management  -----  */
  if (endofcharge)
  {
    dataLogger("finished");
  }
  if (endofcharge || car_is_off)
  {
    esp_light_sleep_start();
  }

  /*  -----  Intervals  -----  */
  if (since_int1 > int1)
  {
    handleEvse();
    since_int1 -= int1;
  }

  if (since_int2 > int2)
  {
    dataLogger();
    since_int2 -= int2;
  }
}