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

long int1 = 30;  // don't touch!!
long int2 = 120; // 2 min datalogging interval
long int3 = 2000;
long unsigned time_millis = 0;
long time_minutes = 0;
long time_seconds = 0;
long millis_sincestart = 0;

typedef int32_t esp_err_t;

File serialfile;
File logfile;
int logfile_nr = EEPROM.readInt(0);

// states
int car_mode = CAR_MODE::UNKNOWN;
int bms_state = BMS_STATE::NORMAL;
int bms_req = BMS_REQ::READY;

// battery heating
float ptc_temp = 0;
float ptc_temp_setp = 35; // degrees celsius
bool heating_en = false;
int pwm_ptc = 0;
const float WARMUP_TEMP = 25; // heat battery up to this temperature

// bms
float celltemp_front = 0;
float celltemp_rear = 0;
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
String str_ctmp_front; // cell temperature front
String str_ctmp_rear;
String str_soc;
String str_dc_amps;
String str_max_cable_amps;
String str_max_evse_amps;
String str_bal_p;
String str_bal_cap;
String str_ptc_temp;
String str_ptc_temp_sp;
String str_heating_en;
String str_pwm_ptc;

bool charging_started = false;
bool ccu_inactive = false;
long since_inactive = 0;
bool request_t = true;
string request_state;

/* PWM channels */
const uint8_t chargerpwm_ch = 1;
const uint8_t greenled_ch = 2;
const uint8_t ptc_ch = 3;

OneWire oneWire(DS18B20_BUS);
DallasTemperature sensor(&oneWire);

void thermalManagement(); // take care of heating the battery

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
  ledcSetup(greenled_ch, 1000, 8);    // channel, freq, res
  ledcSetup(ptc_ch, 5, 8);         // channel, freq, res

  ledcAttachPin(PWM, chargerpwm_ch);
  ledcAttachPin(S1_LED_GREEN, greenled_ch);
  ledcAttachPin(S2_PTC_ON, ptc_ch);

  adcAttachPin(EVSE_PROX);
  analogReadResolution(10);

  attachInterrupt(EVSE_PILOT, risingIRQ, RISING); // for EVSE pwm capture

  delay(1000);
  sensor.begin(); // start ds18b20 ptc temp sensor bus
  //Serial.print(sensor.getDeviceCount(), DEC);

  //CAN.setPins(CAN_RX, CAN_TX);
  //CAN.begin(125E3); // 125 kbps

  serial_string.reserve(3500);

  digitalWrite(EVSE_STATE_C, HIGH);
  millis_sincestart = millis();
}

void loop()
{
  ArduinoOTA.handle();
  time_millis = millis();
  time_minutes = (time_millis - millis_sincestart) / 60000UL;
  time_seconds = (time_millis - millis_sincestart) / 1000UL;


  /*  -----  Updating data from BMS  -----  */

  if ((car_mode == DRIVE && webserver_active) || car_mode != DRIVE)
  {
    GetSerialData(); // check the state of serial port

    if (bms_req == READY)
    {
      if (request_t) // switch between requesting 't' and 'd' when balancing
      {
        GetSerialData("t"); // get general data from bms
      }
      else
      {
        GetSerialData("d"); // get balancing data from bms
      }
    }
  }



  if (evse.is_plugged_in || car_mode == CHARGE)
  {
    /*  -----  Control charger  -----  */
    if (bms_req == RECEIVED)
    {
      ControlCharger();
    }
    else if (bms_req == TIMEOUT || bms_req == PARSE_FAIL)
    {
      ControlCharger(false); // turn off charger
    }
    if (bms_state != ENDOFCHARGE && charging_started == false)
    {
      dataLogger("start"); // print a start in logfile
      charging_started = true;
    }
  }
  else
  {
    charging_started = false; // otherwise datalogger doesn't restart
  }

  if (bms_state == ENDOFCHARGE && charging_started) // print a finish in logfile
  {
    dataLogger("finished");
    charging_started = false;
  }


  
  /*  -----  Stringify variables for webserver  -----  */
  if (webserver_active)
  {
    str_vmin = String(vmin, 3);
    str_vmax = String(vmax, 3);
    str_vtot = String(vtot, 3);
    str_ctmp_front = String(celltemp_front, 1);
    str_ctmp_rear = String(celltemp_rear, 1);
    str_soc = String(stateofcharge, 3);
    str_dc_amps = String(dc_amps, 3);
    str_max_cable_amps = String(evse.max_cable_amps, 0);
    str_max_evse_amps = String(evse.max_ac_amps, 1);
    str_bal_cap = String(balanced_capacity, 1);
    str_bal_p = String(balancing_power, 1);
    str_ptc_temp = String(ptc_temp, 1);
    str_ptc_temp_sp = String(WARMUP_TEMP, 1);
    str_pwm_ptc = String(pwm_ptc);
    if (heating_en)
    {
      str_heating_en = "true";
    }
    else
    {
      str_heating_en = "false";
    }
  }



  /*  -----  Check webserver activity  -----  */

  if (time_minutes - since_web_req > 2) // if there was no request in the last 2 min
  {
    if (webserver_active)
    {
      Serial.println("Webserver inactive");
      webserver_active = false;
    }
  }

  if (time_minutes - since_web_req <= 2)
  {
    if (webserver_active == false)
    {
      Serial.println("Webserver active");
      webserver_active = true;
    }
  }

  if (evse.is_plugged_in && car_mode == OFF) // turn off evse if finished or car has shut down
  {
    digitalWrite(EVSE_STATE_C, LOW);
  }



  /*  -----  Power management  -----  */

  if ((bms_state == ENDOFCHARGE || car_mode == OFF) && webserver_active == false && evse.is_plugged_in == false) // keep alive if webserver is active and car is active
  {
    if (ccu_inactive == false)
    {
      Serial.print("Controller inactive, entering sleep in 2 minutes");
      since_inactive = time_minutes;
      ccu_inactive = true;
    }

    if (time_minutes - since_inactive >= 2) // 2 mins after shutdown
    {
      Serial.println("Entering sleep now...");
      Serial.flush();
      delay(50);
      digitalWrite(EVSE_STATE_C, LOW);
      ledcWrite(ptc_ch, 0);
      Serial1.end();
      delay(10);

      gpio_set_direction(EVSE_PILOT, GPIO_MODE_INPUT);
      esp_sleep_enable_ext0_wakeup(EVSE_PILOT, HIGH);
      esp_sleep_enable_ext0_wakeup(UART_RX1, LOW);
      delay(10);  

      esp_light_sleep_start();

      esp_restart();
      //esp_deep_sleep_start();
    }
  }
  else
  {
    since_inactive = 0;
    ccu_inactive = false;
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
  sensor.requestTemperatures();
  ptc_temp = sensor.getTempCByIndex(0);
  //Serial.println((String) "temp: " + ptc_temp);

  static int temp_setp_dev = 0;

  if (evse.is_plugged_in && car_mode == CHARGE)
  {
    if (celltemp_front < CELLTEMP_MIN_UPPER || celltemp_rear < CELLTEMP_MIN_UPPER) // start heating once below treshold
    {
      heating_en = true;
    }
  }
  if (evse.is_plugged_in == false || car_mode != CHARGE)
  {
    heating_en = false;
  }

  if (heating_en)
  {
    // regulate ptc temperature
    temp_setp_dev = ptc_temp_setp - celltemp_rear;

    // 80 = +35C
    // 70 = +30C
    // 58 = +30c

    if (ptc_temp < ptc_temp_setp - 1)
    {
      pwm_ptc = 180;
    }
    else
    {
      pwm_ptc = temp_setp_dev * 1.75; // 1.8 works but gets a bit too hots
    }
    
    if (celltemp_front >= WARMUP_TEMP || celltemp_rear >= WARMUP_TEMP)
    {
      ptc_temp_setp = WARMUP_TEMP; // keep battery at temperature
    }

    if (ptc_temp > (ptc_temp_setp + 10)) // prevent overheat
    {
      pwm_ptc = 0;
    }

    // limit duty cycle
    if (pwm_ptc >= 200)
    {
      pwm_ptc = 200;
    }
    if (pwm_ptc <= 0)
    {
      pwm_ptc = 0;
    }

    ledcWrite(ptc_ch, pwm_ptc);
  }
  else
  {
    pwm_ptc = 0;
    ledcWrite(ptc_ch, 0);
  }
}
