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

long int1 = 30; // don't touch!!
long int2 = 120; // 2 min datalogging interval
long int3 = 500;
long unsigned time_millis = 0;
long time_minutes = 0;
long time_seconds = 0;
long millis_sincestart = 0;

typedef int32_t esp_err_t;

File serialfile;
File logfile;
int logfile_nr = EEPROM.readInt(0);

float ptc_temp = 0;
float ptc_temp_setp = 35; // degrees celsius

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
const uint8_t greenled_ch = 2;
const uint8_t ptc_ch = 3;

void thermalManagement();  // take care of heating the battery

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, UART_RX1, UART_TX1);
  Serial1.setRxBufferSize(4096);

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

  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO);
  //esp_sleep_enable_ext0_wakeup(INPUT_S1, LOW);
  esp_sleep_enable_ext0_wakeup(EVSE_PILOT, HIGH);
  esp_sleep_enable_ext0_wakeup(UART_RX1, HIGH);

  gpio_set_pull_mode(INPUT_S1, GPIO_PULLUP_ONLY);

  gpio_set_direction(EVSE_PILOT, GPIO_MODE_INPUT);
  pinMode(EVSE_STATE_C, OUTPUT);
  //gpio_set_direction(EVSE_STATE_C, GPIO_MODE_OUTPUT);
  //gpio_set_direction(S1_LED_GREEN, GPIO_MODE_OUTPUT); // is pwm now
  //gpio_set_direction(S2_PTC_ON, GPIO_MODE_OUTPUT); // is pwm now
  gpio_set_direction(HB_IN1, GPIO_MODE_OUTPUT);
  gpio_set_direction(HB_IN2, GPIO_MODE_OUTPUT);

  ledcSetup(chargerpwm_ch, 1000, 10); // channel, freq, res
  ledcSetup(greenled_ch, 1000, 8); // channel, freq, res
  ledcSetup(ptc_ch, 1000, 8); // channel, freq, res

  ledcAttachPin(PWM, chargerpwm_ch);
  ledcAttachPin(S1_LED_GREEN, greenled_ch);
  ledcAttachPin(S2_PTC_ON, ptc_ch);

  adcAttachPin(EVSE_PROX);
  analogReadResolution(10);

  attachInterrupt(EVSE_PILOT, risingIRQ, RISING); // for EVSE pwm capture

  CAN.setPins(CAN_RX, CAN_TX);
  //CAN.begin(125E3); // 125 kbps

  serial_string.reserve(3500);

  digitalWrite(EVSE_STATE_C, HIGH);
  millis_sincestart = millis();
}

void loop()
{
  time_millis = millis();
  time_minutes = (time_millis - millis_sincestart) / 60000UL;
  time_seconds = (time_millis - millis_sincestart) / 1000UL;

  ArduinoOTA.handle();

  str_vmin = String(vmin, 3);
  str_vmax = String(vmax, 3);
  str_vtot = String(vtot, 3);
  str_ctmp = String(celltemp, 1);
  str_soc = String(stateofcharge, 3);
  str_dc_amps = String(dc_amps, 3);
  str_max_cable_amps = String(evse.max_cable_amps, 0);
  str_max_evse_amps = String(evse.max_ac_amps, 1);


  /*  -----  Updating data from BMS  -----  */
  if ((evse.is_plugged_in || webserver_active) && wifiserial_active == false) // only use rs232 if web is connected or when charging
  {
    static bool first = true;
    if (first == true)
    {
      dataLogger("start"); // start datalogger once
      first = false;
    }

    GetSerialData();
    if (uart_state == BMS_REQ::READY)
    {
      if (request_t) // switch between requesting 't' and 'd' when balancing
      {
        GetSerialData("t"); // get cell voltages from bms
        if (bms_is_balancing)
        {
          request_t = false;
        }
      }
      else
      {
        GetSerialData("d"); // get balancing data from bms
        request_t = true;
      }
    }
    if (uart_state == BMS_REQ::RECEIVED)
    {
      ControlCharger();
    }
    else if (uart_state == BMS_REQ::TIMEOUT || uart_state == BMS_REQ::PARSE_FAIL)
    {
      ControlCharger(false);
    }

    if (rx_timeouts > 20) //20
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
  }
  if (wifiserial_active) // work in progress
  {
    //GetSerialData();
  }




  /*  -----  Power management  -----  */
  static bool prev_state = true;
  if (time_minutes - since_web_req > 2 && webserver_active == true) // if there was no request in the last 5 min
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
  if (time_minutes - since_web_req <= 2) 
  {
    webserver_active = true;
  }

  if (endofcharge) // log the end of charging session
  {
    static bool first = true;
    if (first == true)
    {
      dataLogger("finished");
      first = false;
    }
  }
  if (evse.is_plugged_in && car_is_off) // turn off evse if finished or car has shut down
  {
    digitalWrite(EVSE_STATE_C, LOW); 
  }

  if ((endofcharge || car_is_off) && webserver_active == false && evse.is_plugged_in == false) // keep alive if webserver is active and car is active
  {
    static bool first = true;
    if (first == true)
    {
      Serial.print("Controller inactive, entering sleep in 2 minutes");
      since_inactive = time_minutes;
      first = false;
    }
    if (time_minutes - since_inactive >= 2) // 2 mins after shutdown
    {
      Serial.println("Entering sleep now...");
      Serial.flush();
      delay(50);
      digitalWrite(EVSE_STATE_C, LOW);
      Serial1.end();
      delay(10);

      gpio_set_direction(EVSE_PILOT, GPIO_MODE_INPUT);
      esp_sleep_enable_ext0_wakeup(EVSE_PILOT, HIGH);
      esp_sleep_enable_ext0_wakeup(UART_RX1, LOW);
      delay(10);

      
      esp_light_sleep_start();

      /*
      rx_timeouts = 0;
      trickle_charge = false;
      endofcharge = false;
      bms_is_balancing = false;
      uart_state = READY;
      millis_sincestart = millis();
      */

      esp_restart();
      //esp_deep_sleep_start();
    }
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
    if (evse.is_plugged_in || webserver_active)
    {
      dataLogger();
    }
    
    since_int2 -= int2;
  }

  if (since_int3 > int3)
  {
    thermalManagement();
    since_int3 -= int3;
  }
}


void thermalManagement()
{
  // Get ptc element temperature
  int Vo;
  static float R1 = 10000;
  float logR2, R2;
  static float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;
  
  Vo = analogRead(INPUT_S1);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  ptc_temp = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2));
  ptc_temp = ptc_temp - 273.15;
  ptc_temp = (ptc_temp * 9.0)/ 5.0 + 32.0; 
  Serial.println((String)"temp: " + ptc_temp);


  static int pwm_val = 10;
  static int temp_setp_dev = 0;

  if (car_is_off == false && celltemp < CELLTEMP_MIN_UPPER)
  {
    temp_setp_dev = ptc_temp_setp - ptc_temp;

    if (temp_setp_dev < -2) // ptc is too hot
    {
      pwm_val--;
    }
    if (temp_setp_dev > 2) // ptc is too cold
    {
      pwm_val++;
    }

    constrain(pwm_val, 10, 180); // 70% duty max
    ledcWrite(ptc_ch, pwm_val);
  }
  else
  {
    ledcWrite(ptc_ch, 0);
  }

}
