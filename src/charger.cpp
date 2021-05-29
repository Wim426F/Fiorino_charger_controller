#include <charger.h>
#include <globals.h>
#include <uart_parser.h>
#include <driver/adc.h>

using namespace std;

float charger_pwm = 0;

float ramp_up_exp = 5;
float ramp_down_exp = 3;

/* Limits */
const float CELLTEMP_MIN = 0.0;
const float CELLTEMP_PREFERRED = 10.0;
const float CELLTEMP_MAX = 45.0;

const float VMIN_LIM = 3.000;
const float VMAX_LIM_LOWER = 4.000;
const float VMAX_LIM_UPPER = 4.165;
const float VTOT_LOW = VMIN_LIM * 72;
const float VTOT_MAX = VMAX_LIM_UPPER * 72;

const float CHARGER_MAX_AC_AMPS = 13; // this is required for EVSE

/* Charging states */
bool endofcharge = false;
bool bms_is_balancing = false;
bool trickle_phase = false;

/* EVSE */
float evse_pilot_mvolt = 0;
float evse_prox_mvolt = 0;
float evse_max_ac_amps = 0;
float evse_max_cable_amps = 0;
float ac_amps = 0;
int evse_pilot_pwm = 0;
bool evse_ready = true;

volatile unsigned long pwm_pulse_length = 0;
volatile unsigned long pwm_prev_time = 0;
volatile bool pwm_low = false;
volatile bool pwm_high = false;

void IRAM_ATTR risingIRQ();
void IRAM_ATTR fallingIRQ();

void IRAM_ATTR risingIRQ()
{
  pwm_high = true;
  pwm_low = false;
  pwm_prev_time = micros();
  attachInterrupt(EVSE_PILOT, fallingIRQ, FALLING);
}

void IRAM_ATTR fallingIRQ()
{
  pwm_low = true;
  pwm_high = false;
  pwm_pulse_length = micros() - pwm_prev_time;
  attachInterrupt(EVSE_PILOT, risingIRQ, RISING);
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void dataLogger(string parameter)
{
  if (!SD.exists("/html/index.html"))
  {
    Serial.println("SD card removed!");
    SD.end();
    SD.begin(SS, SPI, 80000000, "/sd", 20);
    delay(500);
    if (SD.exists("/html/index.html"))
    {
      Serial.println("SD card reinitialized");
    }
  }

  if (SD.totalBytes() - SD.usedBytes() < 1000) // clean up sd card if almost full
  {
    for (int i = 0; i < logfile_nr; i++)
    {
      SD.remove("/log/logfile_" + (String)i + ".txt");
    }
  }

  if (logfile.size() > 50000) // max file size 50kB
  {
    EEPROM.writeInt(0, logfile_nr + 1); // new file location
    logfile.close();
    logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_WRITE);
    logfile.print("Time;  Vmin;  Vmax;  Vtot;  Temperature;  SOC;  LEM;  ChargerPwm\n");
    logfile.close();
  }

  if (!SD.exists("/log/logfile_0.txt"))
  {
    logfile = SD.open("/log/logfile_0.txt", FILE_WRITE);
    logfile.println("#Logfile number: " + (String)logfile_nr + "\n");
    logfile.print("Time;  Vmin;  Vmax;  Vtot;  Temperature;  SOC;  LEM;  ChargerPwm\n");
    logfile.close();
    EEPROM.writeInt(0, 0); // reset logfile number
  }

  if (!SD.exists("/log/logfile_" + (String)logfile_nr + ".txt") || parameter == "clear")
  {
    logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_WRITE);
    logfile.println("#Logfile number: " + (String)logfile_nr);
    logfile.print("Time;  Vmin;  Vmax;  Vtot;  Temperature;  SOC;  LEM;  ChargerPwm\n");
    logfile.close();
  }

  if (!logfile.available())
  {
    logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_APPEND);
  }

  if (parameter == "start")
  {
    //Serial.print("\n#New charging session started\n");
    logfile.print("\n#New charging session started\n");
    logfile.print("Time;  Vmin;  Vmax;  Vtot;  Temperature;  SOC;  LEM;  ChargerPwm\n");
  }

  if (parameter == "finished")
  {
    logfile.print("\n#Charging session finished\n");
  }
  else
  {
    //Stupid excel formatting
    str_vmin.replace(".", ",");
    str_vmax.replace(".", ",");
    str_vtot.replace(".", ",");
    str_ctmp.replace(".", ",");
    str_soc.replace(".", ",");
    str_dc_amps.replace(".", ",");
    String str_time_minutes = (String)time_minutes;
    int pwm = (int)charger_pwm;
    String str_pwm = (String)pwm;
    String str_balancing_power = (String)balancing_power;
    str_balancing_power.replace(".", ",");
    String str_balanced_capacity = (String)balanced_capacity;
    str_balanced_capacity.replace(".", ",");

    logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_APPEND);
    logfile.print(str_time_minutes + ";  " + str_vmin + ";  " + str_vmax + ";  " + str_vtot + ";  " + str_ctmp + ";  " + str_soc + ";  " + str_dc_amps + ";  " + str_pwm);
    logfile.flush();

    if (bms_is_balancing == true)
    {
      static int i = 0;
      if (i == 0)
      {
        logfile.print("Balancing_W;  Dissipated_Wh\n");
        i++;
      }
      else
      {
        logfile.print(str_balancing_power + ";  " + str_balanced_capacity);
      }
    }

    logfile.print("\n");
    //Serial.println(str_time_minutes + ";" + str_vmin + ";" + str_vmax + ";" + str_vtot + ";" + str_ctmp + ";" + str_soc + ";" + str_dc_amps + ";" + str_pwm + "\n");
  }
}

void ControlCharger(bool charger_on)
{
  /* Voltage limits & throttling */
  if (vmin > 0 && vmin < VMIN_LIM) // ramping up
  {
    // map difference between max voltage limit and current voltage to 130-915 raised by an exponent of 3
    static float in_max = pow(920 * VMIN_LIM, ramp_up_exp);
    float ratio = pow(920 * vmin, ramp_up_exp);

    charger_pwm = mapFloat(ratio, 0, in_max, 130, 920);
  }

  if (vmin >= VMIN_LIM && vmax <= VMAX_LIM_LOWER) // full speed
  {
    charger_pwm = 920;
  }

  if (vmin >= VMIN_LIM && vmax >= VMAX_LIM_LOWER && vmax <= VMAX_LIM_UPPER) // ramping down
  {
    // map difference between max voltage limit and current voltage to 130-915 raised by an exponent of 3
    static float in_max = pow(920 * (VMAX_LIM_UPPER - VMAX_LIM_LOWER), ramp_down_exp);
    float ratio = pow(920 * (VMAX_LIM_UPPER - vmax), ramp_down_exp);

    charger_pwm = mapFloat(ratio, 0, in_max, 130, 920);
  }

  if (vmin > VMAX_LIM_UPPER || vmax > VMAX_LIM_UPPER || vtot >= VTOT_MAX) // cut-off
  {
    charger_pwm = 0;
  }

  if (vmin == 0 || vmax == 0 || vtot == 0)
  {
    charger_pwm = 0;
  }

  /* Temperature limits & throttling */
  if (celltemp < CELLTEMP_MIN)
  {
    charger_pwm = 0;
  }

  if (celltemp < CELLTEMP_PREFERRED && celltemp > CELLTEMP_MIN) // throttle charger when temperature is lower
  {
    charger_pwm -= (CELLTEMP_PREFERRED - celltemp) * (charger_pwm / 20);
  }

  if (celltemp > CELLTEMP_MAX)
  {
    charger_pwm = 0;
  }

  /* Charger speed limits */
  if (charger_pwm != 0)
  {
    if (trickle_phase == true)
    {
      charger_pwm = 130;
    }

    // EVSE current regulating
    if (evse_pilot_pwm >= 7) // below EVSE duty of 7% is error
    {
      // only throttle down if charger can pull more amps than evse allows
      if (evse_max_ac_amps <= CHARGER_MAX_AC_AMPS)
      {
        // map allowed current with pwm duty cycle
        charger_pwm = map(evse_max_ac_amps, 4, CHARGER_MAX_AC_AMPS, 400, 920);
      }
    }

    charger_pwm = constrain(charger_pwm, 130, 920);
  }

  if (charger_on == false || endofcharge == true)
  {
    charger_pwm = 0;
  }

  ledcWrite(chargerpwm_ch, charger_pwm);
}

void handleEvse()
{
  evse_pilot_mvolt = 0;
  evse_prox_mvolt = 0;
  evse_max_cable_amps = 0;

  // ADC and interrupts don't work together.
  unsigned long sample_start = micros();
  while (pwm_low == true && micros() - sample_start < 80) // wait for pwm pulse to pass with 80us timeout
    ;

  evse_pilot_mvolt = adc1_get_raw(ADC1_CHANNEL_7); // analogRead is not fast enough

  evse_pilot_mvolt = mapFloat(evse_pilot_mvolt, 0, 1023, 0, 3310);
  evse_pilot_mvolt *= 5.371756407f; // voltage calibration and multiplication

  if (micros() - pwm_prev_time > 10000) // 10 millis pwm timeout
  {
    pwm_pulse_length = 0;
  }

  evse_pilot_pwm = pwm_pulse_length / 10;
  if (evse_pilot_pwm != 0)
  {
    evse_pilot_pwm = constrain(evse_pilot_pwm, 7, 96); // 7% = 4.5A - 96% = 80A
  }

  evse_prox_mvolt = analogReadMilliVolts(EVSE_PROX) * 1.047915f; // calibration value

  ac_amps = (dc_amps * vtot) / 230 / 3 / 0.9f; // charger efficiency is ~90%

  // max allowed EVSE phase current
  evse_max_ac_amps = evse_pilot_pwm * 0.6f;

  // Detecting cable amperage rating
  if (evse_prox_mvolt > 160)
  {
    evse_max_cable_amps = 63;
  }
  if (evse_prox_mvolt > 430)
  {
    evse_max_cable_amps = 32;
  }
  if (evse_prox_mvolt > 850)
  {
    evse_max_cable_amps = 20;
  }
  if (evse_prox_mvolt > 1650)
  {
    evse_max_cable_amps = 13;
  }
  if (evse_prox_mvolt > 3000)
  {
    evse_max_cable_amps = 0;
  }

  Serial.println((String) "max_ac_amps: " + evse_max_ac_amps + "  pilot_pwm: " + evse_pilot_pwm + "  pilot_mV: " + evse_pilot_mvolt + "  cable-amps: " + evse_max_cable_amps);
}