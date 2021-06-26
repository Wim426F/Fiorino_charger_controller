// lines: 1196
// headers
#include <charger.h>
#include <globals.h>
#include <web_server.h>
#include <uart_parser.h>
#include <can_parser.h>
using namespace std;

/* Timers */
elapsedMillis since_int1 = 0;
elapsedSeconds since_int2 = 0;
elapsedMillis since_int3 = 0;

long int1 = 30;
long int2 = 300; // 5 min datalogging interval
long int3 = 100;
long time_minutes = 0;
long time_seconds = 0;

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
String str_max_cable_amps;
String str_max_evse_amps;

bool car_is_off = false;
long since_car_is_off = 0;
long since_inactive = 0;
bool request_t = true;
string request_state;

/* PWM channels */
const uint8_t chargerpwm_ch = 1;

void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();
  delay(2000);
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    Serial.println("Wakeup caused by external signal using RTC_IO");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    Serial.println("Wakeup caused by external signal using RTC_CNTL");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    Serial.println("Wakeup caused by timer");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    Serial.println("Wakeup caused by touchpad");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    Serial.println("Wakeup caused by ULP program");
    break;
  default:
    Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, UART_RX1, UART_TX1);
  Serial1.setRxBufferSize(4096);

  print_wakeup_reason();

  SD.begin(SD_CS, SPI, 80000000, "/sd", 15);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_ip, gateway, subnet);
  WiFi.softAP(ap_ssid, password);

  //Over The Air update
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.begin();
  StartMdnsService();
  ConfigWebServer();
  //initWebSocket();

  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO);
  esp_sleep_enable_uart_wakeup(0);
  esp_sleep_enable_uart_wakeup(1);
  esp_sleep_enable_ext0_wakeup(INPUT_S1, LOW);
  esp_sleep_enable_ext0_wakeup(EVSE_PILOT, HIGH);

  gpio_set_pull_mode(INPUT_S1, GPIO_PULLUP_ONLY);

  gpio_set_direction(EVSE_PILOT, GPIO_MODE_INPUT);
  pinMode(EVSE_STATE_C, OUTPUT);
  //gpio_set_direction(EVSE_STATE_C, GPIO_MODE_OUTPUT);
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

  digitalWrite(EVSE_STATE_C, HIGH);
}

void loop()
{
  time_minutes = millis() / 60000UL;
  time_seconds = millis() / 1000UL;

  ArduinoOTA.handle();

  input_s1 = gpio_get_level(INPUT_S1);

  str_vmin = String(vmin, 3);
  str_vmax = String(vmax, 3);
  str_vtot = String(vtot, 3);
  str_ctmp = String(celltemp, 1);
  str_soc = String(stateofcharge, 3);
  str_dc_amps = String(dc_amps, 3);
  str_max_cable_amps = String(evse.max_cable_amps, 0);
  str_max_evse_amps = String(evse.max_ac_amps, 1);

  /*  -----  Updating data from BMS  -----  */
  GetSerialData();
  if (uart_state == BMS_REQ::READY)
  {
    if (request_t) // switch between requesting 't' and 'd' when balancing
    {
      GetSerialData("t"); // get cell voltages from bms
    }
    else
    {
      GetSerialData("d"); // get balancing data from bms
      request_t = true;
    }
    if (bms_is_balancing)
    {
      request_t = false;
    }
  }
  if (uart_state == BMS_REQ::RECEIVED)
  {
    ControlCharger();
  }
  if (uart_state == BMS_REQ::TIMEOUT || uart_state == BMS_REQ::PARSE_FAIL)
  {
    ControlCharger(false);
  }

  if (rx_timeouts > 20)
  {
    static bool first = true;
    if (first == true)
    {
      Serial.println("Car turned of");
      dataLogger("car turned of");
      car_is_off = true; // if request times out, the car has probably shutdown
      since_car_is_off = time_minutes;
      first = false;
    }
  }
  else
  {
    car_is_off = false;
  }

  /*  -----  Power management  -----  */
  static bool prev_state = true;
  if (time_minutes - since_web_req > 5 && webserver_active == true) // if there was no request in the last 5 min
  {
    webserver_active = false;
    static bool first = true;
    if (first == true)
    {
      Serial.println("Webserver inactive");
      first = false;
    }
    prev_state = true;
  }
  if (time_minutes - since_web_req <= 5)
  {
    webserver_active = true;
  }

  if (endofcharge)
  {
    dataLogger("finished");
  }
  if ((endofcharge || car_is_off || evse.is_plugged_in == false) && webserver_active == false)
  {
    static bool first = true;
    if (first == true)
    {
      Serial.print("Controller inactive, entering deep sleep in 2 minutes");
      since_inactive = time_minutes;
      first = false;
    }
    if (time_minutes - since_inactive >= 2) // 2 mins after shutdown
    {
      Serial.println("Entering deep sleep now...");
      Serial.flush();
      delay(50);
      // reset some variables
      /*
      car_is_off = false;
      webserver_active = true;
      since_inactive = time_minutes;
      since_car_is_off = time_minutes;
      rx_timeouts = 0;
      rx_waiting = false; */
      digitalWrite(EVSE_STATE_C, LOW);
      esp_sleep_enable_ext0_wakeup(EVSE_PILOT, HIGH);
      delay(10);
      //esp_light_sleep_start();
      //esp_restart();
      esp_deep_sleep_start();
    }
  }

  if (input_s1 == LOW)
  {
  }

  /*  -----  Intervals  -----  */
  if (since_int1 > int1)
  {
    getEvseParams();
    handleEvse();
    since_int1 -= int1;
  }

  if (since_int2 > int2)
  {
    dataLogger();
    since_int2 -= int2;
  }

  if (since_int3 > int3)
  {
    since_int3 -= int3;
  }
}