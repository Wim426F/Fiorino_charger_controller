#include <uart_parser.h>
#include <charger.h>
#include <globals.h>

using namespace std;

/* Variables */
string serial_string;

const int rx_timeout = 7000; // millis
int request_intv = 3000;     // update values every 3000ms
bool incoming = false;

unsigned long since_answer = 0;
unsigned long since_request = 0;
unsigned long since_byte1 = 0;

int rx_timeouts = 0;

void WifiSerial();

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

void GetSerialData(string input)
{
  unsigned long time_millis = millis();

  if (bms_state == TRICKLE_CHARGE)
  {
    request_intv = 15000; // too frequent refreshing of data during trickle phase doesn't work
  }

  if (time_millis - since_answer > request_intv && bms_req != WAITING) // set ready state after interval
  {
    bms_req = READY;
  }

  if (bms_req == READY && input != "")
  {
    while (Serial1.available() && Serial1.read())
      ; // empty buffer again

    Serial1.println(input.c_str());
    Serial.println(input.c_str());
    bms_req = WAITING;
    since_request = time_millis;
  }
  else // wait for incoming data
  {
    if (time_millis - since_request > rx_timeout && bms_req == WAITING) // timeout
    {
      Serial.println("No data received: timeout");
      bms_req = TIMEOUT;
      since_answer = time_millis;
      rx_timeouts++;

      if (rx_timeouts > 5) // if request times out, the car has probably shutdown
      {
        if (car_mode != OFF)
        {
          Serial.println("Car turned of");
          dataLogger("car turned of");
          car_mode = OFF;
        }
      }

      stateofcharge = 0;
      dc_amps = 0;
      vmin = 0;
      vmax = 0;
      vtot = 0;
      celltemp_front = 0;
      celltemp_rear = 0;
    }

    if (Serial1.available() > 0)
    {
      if (incoming == false) // record time when first byte arrives
      {
        since_byte1 = time_millis;
        incoming = true;
      }

      if (time_millis - since_byte1 > 280) // read string from buffer 280ms after first byte
      {
        serial_string.clear();

        while (Serial1.available() > 0)
        {
          delayMicroseconds(72); // 1 byte takes 69us to arrive
          serial_string += char(Serial1.read());
        }

        since_answer = time_millis;

        incoming = false;
        bms_req = BMS_REQ::RECEIVED;

        Serial.println("BMS data received!");
        //Serial.println(serial_string.c_str());
        WifiSerial();

        string return_state;

        if (request_t) // switch between t and d when bms is balancing
        {
          return_state = ParseStringData("t");
          if (bms_state == BALANCING)
          {
            request_t = false;
          }
        }
        else
        {
          return_state = ParseStringData("d");
          request_t = true;
        }

        if (return_state == "fail") // if data is corrupt, the rs232 port is probably in use
        {
          bms_req = BMS_REQ::PARSE_FAIL;
          Serial.println("Data corrupted");
        }
      }
      rx_timeouts = 0;
    }
  }
}

string ParseStringData(std::string input)
{
  string output;

  remove(serial_string.begin(), serial_string.end(), ' '); // remove spaces in string

  if (input == "t")
  {
    stateofcharge = 0;
    dc_amps = 0;
    vmin = 0;
    vmax = 0;
    vtot = 0;
    celltemp_front = 0;
    celltemp_rear = 0;

    if (serial_string.find("Marcia") != -1) // Car is in drive mode
    {
      car_mode = DRIVE;
    }
    else // car is in charge mode
    {
      car_mode = CHARGE;
    }

    if (serial_string.find("SOC") != -1)
    {
      // Cell temperature front (bms 5)
      size_t celltemp_idx = serial_string.find("-7f") + 21;
      if (serial_string[celltemp_idx] == '-')
        celltemp_front = stof(serial_string.substr(celltemp_idx, 4));
      if (serial_string[celltemp_idx] == '.')
        celltemp_front = stof(serial_string.substr(celltemp_idx, 2));
      if (serial_string[celltemp_idx] != '-' || serial_string[celltemp_idx] != '.')
        celltemp_front = stof(serial_string.substr(celltemp_idx, 4));

      // Cell temperature rear (bms 2)
      celltemp_idx = 0;
      celltemp_idx = serial_string.find("-3f") + 21;
      if (serial_string[celltemp_idx] == '-')
        celltemp_rear = stof(serial_string.substr(celltemp_idx, 4));
      if (serial_string[celltemp_idx] == '.')
        celltemp_rear = stof(serial_string.substr(celltemp_idx, 2));
      if (serial_string[celltemp_idx] != '-' || serial_string[celltemp_idx] != '.')
        celltemp_rear = stof(serial_string.substr(celltemp_idx, 4));

      // Total voltage
      size_t vtot_idx = serial_string.find("Vtot") + 5;
      if (vtot_idx != 0)
      {
        vtot = stof(serial_string.substr(vtot_idx, vtot_idx + 6));
      }

      // SOC
      size_t stateofcharge_idx = serial_string.find("SOC") + 4;
      if (stateofcharge_idx != 0)
      {
        stateofcharge = stof(serial_string.substr(stateofcharge_idx, stateofcharge_idx + 4));
      }

      // Current Sensor
      size_t lemsensor_idx = serial_string.find("LEM") + 4;
      if (lemsensor_idx != 0)
      {
        dc_amps = stof(serial_string.substr(lemsensor_idx, lemsensor_idx + 4));
      }

      output = "succes";
    }
    else
    {
      output = "fail";
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

    if (vmin_idx != 0 && vmax_idx != 0)
    {
      vmin = stof(serial_string.substr(vmin_idx, vmin_idx + 5));
      vmax = stof(serial_string.substr(vmax_idx, vmax_idx + 5));
    }

    if (serial_string.find("Equilibratura") != -1)
    {
      bms_state = BALANCING;
    }

    if (vmax >= VMAX_LIM_UPPER)
    {
      bms_state = BMS_STATE::TRICKLE_CHARGE;
    }

    if (vmax >= VMAX_LIM_UPPER && stateofcharge >= 70.0 && vmin >= (VMAX_LIM_UPPER - 0.02))
    {
      if (bms_state != BALANCING)
      {
        bms_state = ENDOFCHARGE;
      }
      else if (bms_state == BALANCING && balancing_power > 0.1 && balancing_power < 1)
      {
        bms_state = ENDOFCHARGE;
      }
    }

    Serial.print("vmin: ");
    Serial.println(vmin);
    Serial.print("vmax: ");
    Serial.println(vmax);
    Serial.print("vtot: ");
    Serial.println(vtot);
    Serial.print("Temperature front: ");
    Serial.println(celltemp_front);
    Serial.print("Temperature rear: ");
    Serial.println(celltemp_rear);
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
      size_t balanced_capacity_idx = serial_string.find("dissipata") + 10;
      balanced_capacity = stof(serial_string.substr(balanced_capacity_idx, 5));
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
      size_t balancing_power_idx = serial_string.find("istantanea") + 11;
      balancing_power = stof(serial_string.substr(balancing_power_idx, 5));
      Serial.print("Balancing Power: ");
      Serial.println(balancing_power);
    }
  }

  serial_string.clear();
  return output;
}

void WifiSerial()
{
  if (serialfile.size() > 30000) // max file size 30kB
  {
    serialfile.close();
    serialfile = SD.open("/log/serial.txt", FILE_WRITE);
    serialfile.print("");
    serialfile.close();
  }

  if (!SD.exists("/log/serial.txt"))
  {
    logfile = SD.open("/log/serial.txt", FILE_WRITE);
    logfile.print("");
    logfile.close();
  }
  else
  {
    serialfile = SD.open("/log/serial.txt", FILE_WRITE);
    serialfile.println(serial_string.c_str());
    serialfile.close();
  }
}