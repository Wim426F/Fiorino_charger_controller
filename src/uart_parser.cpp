#include <uart_parser.h>
#include <globals.h>
#include <charger.h>

using namespace std;

/* Variables */
string serial_string;

const int rx_timeout = 7000; // millis
int request_intv = 1000;     // update values every 1000ms
bool rx_waiting = true;
bool incoming = false;
bool data_received = false;

long since_answer = 0;
long since_request = 0;
long since_byte1 = 0;

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

string GetSerialData(string input) // returns "succes", "waiting" or "timeout"
{
  static string output;

  if (trickle_phase == true)
  {
    request_intv = 20000; // too frequent refreshing data during trickle phase doesn't work well
  }

  if (rx_waiting == false && millis() - since_answer > request_intv) // send data after interval
  {
    serial_string.clear();

    while (Serial1.available() && Serial1.read())
      ; // empty buffer again

    Serial1.println(input.c_str());
    Serial.println(input.c_str());
    rx_waiting = true;
    since_request = millis();
    output = "waiting";
  }
  else // wait for incoming data
  {
    if (millis() - since_request > rx_timeout && rx_waiting == true)
    {
      Serial.println("No data received: timeout");
      data_received = false;
      rx_waiting = false;
      output = "timeout";
    }

    if (Serial1.available() > 0) 
    {
      if (incoming == false) // record time when first byte arrives
      {
        since_byte1 = millis();
        incoming = true;
      }

      if (millis() - since_byte1 > 280) // read string from buffer 280ms after first byte
      {
        while (Serial1.available() > 0)
        {
          serial_string += char(Serial1.read());
        }

        std::remove(serial_string.begin(), serial_string.end(), ' '); // remove spaces in string

        data_received = true;
        rx_waiting = false;
        incoming = false;

        since_answer = millis();
        Serial.println("BMS data received!");
        ParseStringData(input);
        output = "succes";
      }
    }
  }
  return output;
}

string ParseStringData(std::string input)
{
  string output;

  if (input == "t")
  {
    // Cell temperature
    if (serial_string.find("-7f") != -1)
    {
      size_t celltemp_idx = serial_string.find("-7f") + 21;
      if (serial_string[celltemp_idx] == '-')
        celltemp = stof(serial_string.substr(celltemp_idx, celltemp_idx + 4));
      if (serial_string[celltemp_idx] == '.')
        celltemp = stof(serial_string.substr(celltemp_idx, celltemp_idx + 2));
      if (serial_string[celltemp_idx] != '-' || serial_string[celltemp_idx] != '.')
        celltemp = stof(serial_string.substr(celltemp_idx, celltemp_idx + 4));

      output = "succes";
    }
    else
    {
      output = "fail";
    }

    // Total voltage
    size_t vtot_idx = serial_string.find("Vtot") + 5;
    vtot = 0;
    if (vtot_idx != 0)
    {
      vtot = stof(serial_string.substr(vtot_idx, vtot_idx + 6));
    }

    // SOC
    size_t stateofcharge_idx = serial_string.find("SOC") + 4;
    stateofcharge = 0;
    if (stateofcharge_idx != 0)
    {
      stateofcharge = stof(serial_string.substr(stateofcharge_idx, stateofcharge_idx + 4));
    }

    // Current Sensor
    size_t lemsensor_idx = serial_string.find("LEM") + 4;
    dc_amps = 0;
    if (lemsensor_idx != 0)
    {
      dc_amps = stof(serial_string.substr(lemsensor_idx, lemsensor_idx + 4));
    }

    // Min and max cell voltages
    size_t vmin_idx = 0, vmax_idx = 0;
    if (serial_string.find("Vmin") != -1 && serial_string.find("Vmax") != -1)
    {
      vmin_idx = serial_string.find("Vmin:") + 5;
      vmax_idx = serial_string.find("Vmax:") + 5;
    }
    if (serial_string.find("Shunt") != -1) // Shunt is similar to vmax
    {
      vmin_idx = serial_string.find("Vmed:") + 5;
      vmax_idx = serial_string.find("Shunt:") + 6;
    }
    vmin = 0;
    vmax = 0;

    if (vmin_idx != 0 && vmax_idx != 0)
    {
      vmin = stof(serial_string.substr(vmin_idx, vmin_idx + 5));
      vmax = stof(serial_string.substr(vmax_idx, vmax_idx + 5));
    }

    if (serial_string.find("Equilibratura") != -1)
    {
      bms_is_balancing = true;
    }
    else
    {
      bms_is_balancing = false;
    }

    if (vmax >= VMAX_LIM_UPPER)
    {
      trickle_phase = true;
    }

    if (vmax >= VMAX_LIM_UPPER && stateofcharge == 70.0f && vmin >= VMAX_LIM_UPPER && vtot >= VTOT_MAX)
    {
      if (bms_is_balancing == false)
      {
        endofcharge = true;
      }
      if (bms_is_balancing == true && balancing_power > 0.1 && balancing_power < 1)
      {
        endofcharge = true;
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
    Serial.println(dc_amps);
  }

  if (input == "d")
  {
    balanced_capacity = 0;
    balancing_power = 0;

    if (serial_string.find("dissipata") != -1)
    {
      size_t balanced_capacity_idx = serial_string.find("dissipata") + 9;
      balanced_capacity = stof(serial_string.substr(balanced_capacity_idx, balanced_capacity_idx + 6));
      Serial.print("Balanced Capacity: ");
      Serial.println(balanced_capacity);
      output = "succes";
    }
    else
    {
      output = "fail";
    }

    if (serial_string.find("istantanea") != -1)
    {
      size_t balancing_power_idx = serial_string.find("istantanea") + 10;
      balancing_power = stof(serial_string.substr(balancing_power_idx, balancing_power_idx + 4));
      Serial.print("Balancing Power: ");
      Serial.println(balancing_power);
    }
  }

  if (input != "t" && input != "d")
  {
    output = "fail";
  }

  return output;
}
