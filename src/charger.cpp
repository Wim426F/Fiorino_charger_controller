#include <charger.h>
#include <globals.h>

using namespace std;

/* Variables */
int rx_timeout = 0;

string status_str;
string serial_str;

float celltemp = 0;
float stateofcharge = 0;
float lemsensor = 0;
float vmin = 0;
float vmax = 0;
float vtot = 0;
float balancing_power = 0;
float balanced_capacity = 0;
float status = 0;
float charger_duty = 0;

/* Limits */
const float CELLTEMP_MIN = 0.0;
const float CELLTEMP_PREFERRED = 10.0;
const float CELLTEMP_MAX = 45.0;

const float VMIN_LIM = 3.000;
const float VMAX_LIM_LOWER = 4.000;
const float VMAX_LIM_UPPER = 4.150;
const float VTOT_LOW = VMIN_LIM * 72;
const float VTOT_MAX = VMAX_LIM_UPPER * 72;
const float WH_DISSIPATED_MAX = 2000.00;

/* Triggers */
bool endofcharge = false;
bool evse_on = false;
bool charge_limited = false;
bool is_balancing = false;

inline float stof(const string &_Str, size_t *_Idx = nullptr) // convert string to float
{
  int &_Errno_ref = errno; // Nonzero cost, pay it once
  const char *_Ptr = _Str.c_str();
  char *_Eptr;
  _Errno_ref = 0;
  const float _Ans = strtof(_Ptr, &_Eptr);

  if (_Idx)
  {
    *_Idx = static_cast<size_t>(_Eptr - _Ptr);
  }

  return _Ans;
}

void dataLogger()
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
    else
    {
      Serial.println("Failed to reinitialize SD card");
    }
  }

  if (SD.totalBytes() - SD.usedBytes() < 1000)
  {
    for (int i = 0; i < logfile_nr; i++)
    {
      SD.remove("/log/logfile_" + (String)i + ".txt");
    }
  }

  if (logfile.size() > 2000000) // max file size 2MB
  {
    EEPROM.writeInt(0, logfile_nr + 1); // new file location
    logfile.close();
    logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_WRITE);
  }

  if (!SD.exists("/log/logfile_0.txt"))
  {
    logfile = SD.open("/log/logfile_0.txt", FILE_WRITE);
    logfile.print("Time,Vmin,Vmax,Vtot,Temperature,SOC,LEM,ChargerPwm\n");
    EEPROM.writeInt(0, 0);
  }

  if (endofcharge == true)
  {
    logfile.print("\n-- Charging session finished --\n\n");
    logfile.close();
  }

  static int logs_since_start = 0;
  if (logs_since_start == 0)
  {
    logfile.println("\n-- New charging session started --\n");
  }
  logs_since_start++;

  if (is_balancing == true)
  {
    static int i = 0;
    if (i == 0)
    {
      logfile.println("\n-- BMS has started balancing --\n");
    }
    i++;
  }
  logfile = SD.open("/log/logfile_" + (String)logfile_nr + ".txt", FILE_APPEND);
  logfile.print((String)time_minutes + "," + vmin + "," + vmax + "," + vtot + "," + celltemp + "," + stateofcharge + "," + lemsensor + "," + (int)charger_duty + "\n");
  logs_since_start++;
}

string GetSerialData(string input)
{
  string output;
  serial_str.clear();
  serial_str.reserve(3500);

  while (Serial1.available() != -1 && rx_timeout < 10)
  {
    byte trash = Serial1.read();
    rx_timeout++;
    delay(5);
  }

  if (input.length() < 2)
  {
    Serial1.println(input.c_str());
    Serial.println(input.c_str());
  }
  while (!Serial1.available() && rx_timeout <= 300)
  {
    rx_timeout++;
    delay(100);
  }
  if (rx_timeout >= 300)
  {
    rx_timeout = 0;
    Serial.println("No data received: timeout");
    output = "Fail";
  }
  else
  {
    rx_timeout = 0;
    Serial.println("BMS data received!");
    output = "Succes";

    while (Serial1.available() > 0)
    {
      serial_str += char(Serial1.read());
      delay(2); // very neccesary
    }

    remove(serial_str.begin(), serial_str.end(), ' ');
    //serial_str.resize(2040);
  }

  return output;
}

string ParseStringData()
{
  string output;

  if (GetSerialData() == "Succes")
  {
    // Cell temperature
    size_t celltemp_idx = serial_str.find("-7f") + 21;
    if (serial_str[celltemp_idx] == '-')
      celltemp = stof(serial_str.substr(celltemp_idx, celltemp_idx + 4));
    if (serial_str[celltemp_idx] == '.')
      celltemp = stof(serial_str.substr(celltemp_idx, celltemp_idx + 2));
    if (serial_str[celltemp_idx] != '-' || serial_str[celltemp_idx] != '.')
      celltemp = stof(serial_str.substr(celltemp_idx, celltemp_idx + 4));

    // Total voltage
    size_t vtot_idx = serial_str.find("Vtot") + 5;
    vtot = 0;
    vtot = stof(serial_str.substr(vtot_idx, vtot_idx + 6));

    // SOC
    size_t stateofcharge_idx = serial_str.find("SOC") + 4;
    stateofcharge = 0;
    stateofcharge = stof(serial_str.substr(stateofcharge_idx, stateofcharge_idx + 4));

    // Current Sensor
    size_t lemsensor_idx = serial_str.find("LEM") + 4;
    lemsensor = 0;
    lemsensor = stof(serial_str.substr(lemsensor_idx, lemsensor_idx + 4));

    // Min and max cell voltages
    size_t vmin_idx = 0, vmax_idx = 0;
    if (serial_str.find("Vmin") != -1 && serial_str.find("Vmax") != -1)
    {
      vmin_idx = serial_str.find("Vmin:") + 5;
      vmax_idx = serial_str.find("Vmax:") + 5;
    }
    if (serial_str.find("Shunt") != -1) // Shunt is similar to vmax
    {
      vmin_idx = serial_str.find("Vmed:") + 5;
      vmax_idx = serial_str.find("Shunt:") + 6;
    }
    vmin = 0;
    vmax = 0;
    vmin = stof(serial_str.substr(vmin_idx, vmin_idx + 5));
    vmax = stof(serial_str.substr(vmax_idx, vmax_idx + 5));

    if (serial_str.find("Equilibratura") != -1)
    {
      is_balancing = true;
    }

    if (vmax >= 4.15 && stateofcharge == 70 && vmin > 4.10 && vtot > 295)
    {
      if (is_balancing == false)
      {
        endofcharge == true;
      }
      if (is_balancing == true && balancing_power == 0)
      {
        endofcharge == true;
      }
    }

    Serial.print("vmin: ");
    Serial.println(vmin);
    Serial.print("vmax: ");
    Serial.println(vmax);
    Serial.print("vtot: ");
    Serial.println(vtot);
    Serial.print("Temperature: ");
    Serial.println(celltemp);
    Serial.print("State of Charge: ");
    Serial.println(stateofcharge);
    Serial.print("Current sensor: ");
    Serial.println(lemsensor);
    output = "Succes";
  }
  else
  {
    output = "Fail";
  }

  if (is_balancing == true)
  {
    if (GetSerialData("d") == "Succes") // balancing data
    {
      balanced_capacity = 0;
      balancing_power = 0;

      size_t balanced_capacity_idx = serial_str.find("dissipata") + 9;
      balanced_capacity = stof(serial_str.substr(balanced_capacity_idx, balanced_capacity_idx + 6));

      size_t balancing_power_idx = serial_str.find("istantanea") + 10;
      balancing_power = stof(serial_str.substr(balancing_power_idx, balancing_power_idx + 4));

      Serial.print("Balancing Power: ");
      Serial.println(balancing_power);
      Serial.print("Balanced Capacity: ");
      Serial.println(balanced_capacity);

      output = "Succes";
    }
    else
    {
      output = "Fail";
    }
  }
  return output;
}

void ControlCharger(bool charger_on)
{
  /* Voltage limits & throttling */
  if (vmin == 0 || vmax == 0 || vtot == 0)
    charger_duty = 0;

  if (vmin > 0 && vmin < VMIN_LIM)
    charger_duty = 915 - (VMIN_LIM - vmin) * 1000 - 150;

  if (vmin >= VMIN_LIM && vmax <= VMAX_LIM_LOWER)
    charger_duty = 910;

  if (vmin >= VMIN_LIM && vmax >= VMAX_LIM_LOWER && vmax <= VMAX_LIM_UPPER)
    charger_duty = (VMAX_LIM_UPPER - vmax) * 3800 + 100;

  if (vmin > VMAX_LIM_UPPER || vmax > VMAX_LIM_UPPER || vtot >= VTOT_MAX)
    charger_duty = 0;

  /* Temperature limits & throttling */
  if (celltemp < CELLTEMP_MIN)
    charger_duty = 0;

  if (celltemp < CELLTEMP_PREFERRED && celltemp > CELLTEMP_MIN) // throttle charger when temperature is lower
    charger_duty -= (CELLTEMP_PREFERRED - celltemp) * (charger_duty / 20);

  if (celltemp > CELLTEMP_MAX)
    charger_duty = 0;

  /* Balancing */
  if (balanced_capacity > WH_DISSIPATED_MAX)
    charger_duty = 0;

  if ((charger_duty > 0 && charger_duty < 100) || charger_duty < 0)
  {
    charger_duty = 0;
  }

  /* Charger speed limits */
  if (charger_duty != 0)
  {
    if (charger_duty < 170)
      charger_duty = 170;

    if (charger_duty > 915)
      charger_duty = 915;
  }

  if (charger_on = true)
  {
    ledcWrite(chargerpwm_ch, charger_duty);
  }
  else
  {
    ledcWrite(chargerpwm_ch, 0);
  }
}
