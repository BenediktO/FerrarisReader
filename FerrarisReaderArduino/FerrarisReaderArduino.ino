// FerrarisReader - A tool to read old school ferraris energy meters
//
// Copyright (C) 2022  Benedikt Otto <otto@hiskp.uni-bonn.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.



#include "Arduino.h"

#include <ctype.h>

#include "Vrekrer_scpi_parser.h"

//#define DEBUG

SCPI_Parser controller;


// Serial port to use
#define SERIAL Serial

// Number of sensors
#define NUM_CHANNELS 2

// Pin where the LED (via transistor) is connected to
#define IR_LEDS 9
// Pins for the sensors
int IR_SENSORS[NUM_CHANNELS] = {A0, A1};

// Pin where the activity LED is connected to, comment to deactivate
#define ACTIVITY_LED_PIN 10

// default values
#define DEFAULT_UPPER_THRESHOLD 100
#define DEFAULT_LOWER_THRESHOLD 50
#define DEFAULT_DURATION 20

// meter setup
#define TURNS_PER_KWH 75
#define KWH_PER_TURN (1.0 / TURNS_PER_KWH)

// target samplingrate
#define SAMPLINGRATE 250

// For aligning the sensor, uncomment the following line:
//#define ALIGNMENT_MODE



// Do not touch

#ifdef ALIGNMENT_MODE
  #define ALIGNMENT_PIN A0
#endif


#ifdef ACTIVITY_LED_PIN
  #define ACTIVITY_LED_ON digitalWrite(ACTIVITY_LED_PIN, HIGH);
  #define ACTIVITY_LED_OFF digitalWrite(ACTIVITY_LED_PIN, LOW);
#else
  #define ACTIVITY_LED_ON
  #define ACTIVITY_LED_OFF
#endif


bool readout_state = false;
uint8_t second_counter = 0;
uint16_t livetime_counter = 0;
uint16_t livetime_counter_buf = 0;

unsigned long time;
unsigned long last_time;
unsigned long last_time_seconds;


// structure holding the sensor data and parameters
struct SensorData
{
  uint16_t active_value;
  uint16_t dark_value;
  int high_counter;
  int low_counter;
  bool state;
  uint32_t ticks;
  uint32_t last_tick;
  uint32_t second_last_tick;
  uint16_t upper_threshold;
  uint16_t lower_threshold;
  uint16_t duration;
};


struct SensorData sensors[NUM_CHANNELS];


////////////////////////////////////////////////////////////////////////////////
//                                   tools                                    //
////////////////////////////////////////////////////////////////////////////////


// Extracts the channel number from the SCPI parameters, channel is always the first
int get_channel_number(SCPI_P parameters) {
  if (parameters.Size() < 1)
    return 0;
  int temp = String(parameters[0]).toInt();
  // basic validation
  if ((0 <= temp) && (temp < NUM_CHANNELS))
    return temp;
  return 0;
}

// Reset all sensor structs
void reset(void) {
  struct SensorData * sensor;
  for (int i=0; i<NUM_CHANNELS; i++) {
    sensor = &sensors[i];
    sensor->ticks = 0;
    sensor->high_counter = 0;
    sensor->low_counter = 0;
    sensor->state = true;
    sensor->upper_threshold = DEFAULT_UPPER_THRESHOLD;
    sensor->lower_threshold = DEFAULT_LOWER_THRESHOLD;
    sensor->duration = DEFAULT_DURATION;
  }
}

// Returns char array with lower case
char *toLower(char *str, size_t len)
{
    char *str_l = calloc(len + 1, sizeof(char));

    for (size_t i = 0; i < len; ++i) {
        str_l[i] = tolower((unsigned char)str[i]);
    }
    return str_l;
}


////////////////////////////////////////////////////////////////////////////////
//                           SCPI standard commands                           //
////////////////////////////////////////////////////////////////////////////////


// Returns the identication string
void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  interface.println(F("Arduino, FerrarisReader, V1.0, V1.0"));
  ACTIVITY_LED_OFF
}

// Resets the Controller Board
void Reset(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  reset();
  ACTIVITY_LED_OFF
}

// Returns the number of ticks (turns) detected
void MeasureTicks(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.ticks);
  ACTIVITY_LED_OFF
}

// Presets the number of ticks
void SetTicks(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData * sensor;
  if (parameters.Size() >= 1) {
    if (parameters.Size() >= 2) {
      sensor = &sensors[get_channel_number(parameters)];
      sensor->ticks = String(parameters[1]).toInt();
    }
    else {
      sensor = &sensors[0];
      sensor->ticks = String(parameters[0]).toInt();
    }
  }
  ACTIVITY_LED_OFF
}

// Returns the dark current in ADC channels
void MeasureDark(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.dark_value);
  ACTIVITY_LED_OFF
}

// Returns the active current in ADC channels
void MeasureActive(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.active_value);
  ACTIVITY_LED_OFF
}

// Returns the difference of active and dark current in ADC channels 
void MeasureEffective(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.active_value - sensor.dark_value);
  ACTIVITY_LED_OFF
}

// Returns a quantity from 0 to 1 representing the quality of the signal
void MeasureSNR(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(1 - (float) sensor.dark_value / sensor.active_value);
  ACTIVITY_LED_OFF
}

// Returns the amount of energy measured
void MeasureEnergy(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(((float) sensor.ticks) / TURNS_PER_KWH);
  ACTIVITY_LED_OFF
}

// Returns the power measured during the last revolution
void MeasurePower(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  if (sensor.ticks < 2)
    interface.println(F("Invalid"));
  else {
    interface.println(KWH_PER_TURN * 3600000.0 / (sensor.last_tick - sensor.second_last_tick), 4);
  }
  ACTIVITY_LED_OFF
}

// Returns the duration of the last revolution
void MeasurePeriod(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  if (sensor.ticks < 2)
    interface.println(F("Invalid"));
  else {
    interface.println((sensor.last_tick - sensor.second_last_tick) / 1000.0);
  }
  ACTIVITY_LED_OFF
}

// Returns the time that has elapsed since the last tick
void MeasureLastTick(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  if (sensor.ticks < 1)
    interface.println(F("Invalid"));
  else
    interface.println((millis() - sensor.last_tick) / 1000.0);
  ACTIVITY_LED_OFF
}

// Sets the parameters upper and lower threshold
void ConfSetThreshold(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData * sensor;
  uint16_t value;
  if (parameters.Size() >= 1) {
    if (parameters.Size() >= 2) {
      sensor = &sensors[get_channel_number(parameters)];
      value = String(parameters[1]).toInt();
    }
    else {
      sensor = &sensors[0];
      value = String(parameters[0]).toInt();
    }
  if (strcmp(toLower(commands[3], 3), "upp") == 0)
    sensor->upper_threshold = value;
  else if (strcmp(toLower(commands[3], 3), "low") == 0)
    sensor->lower_threshold = value;
  }
  ACTIVITY_LED_OFF
}

// Returns the parameters upper and lower threshold
void ConfGetThreshold(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  if (strcmp(toLower(commands[3], 3), "upp") == 0)
    interface.println(sensor.upper_threshold);
  else if (strcmp(toLower(commands[3], 3), "low") == 0)
    interface.println(sensor.lower_threshold);
  ACTIVITY_LED_OFF
}

// Sets the parameter duration
void ConfSetDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData * sensor;
  if (parameters.Size() >= 1) {
    if (parameters.Size() >= 2) {
      sensor = &sensors[get_channel_number(parameters)];
      sensor->duration = String(parameters[1]).toInt();
    }
    else {
      sensor = &sensors[0];
      sensor->duration = String(parameters[0]).toInt();
    }
  }
  ACTIVITY_LED_OFF
}

// Returns the parameter duration
void ConfGetDuration(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.duration);
  ACTIVITY_LED_OFF
}

// Returns the number of channels of the device
void CountChannels(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  interface.println(NUM_CHANNELS);
  ACTIVITY_LED_OFF
}

// Returns the number of samples during the last second
void GetFrequency(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  ACTIVITY_LED_ON
  interface.println(livetime_counter_buf);
  ACTIVITY_LED_OFF
}

// One tick has occurred, update data.
void tick(struct SensorData * sensor_data) {
  sensor_data->ticks++;
  sensor_data->second_last_tick = sensor_data->last_tick;
  sensor_data->last_tick = millis();
}

// Handle a new pair of active/dark measurement
void handle_measurement(struct SensorData * sensor_data) {
  int difference = sensor_data->active_value - sensor_data->dark_value;

  if (difference > sensor_data->upper_threshold) {
    sensor_data->high_counter++;
    if (sensor_data->high_counter > sensor_data->duration) {
      if (!sensor_data->state)
          tick(sensor_data);    // active -> inactive transition
      sensor_data->state = true;
    }
  }
  else {
    sensor_data->high_counter = 0;
    if (difference < sensor_data->lower_threshold) {
      sensor_data->low_counter++;
      if (sensor_data->low_counter > sensor_data->duration)
        sensor_data->state = false;
    }
    else {
      sensor_data->low_counter = 0;
    }
  }
}



void setup() {
  // SCPI standard commands
  controller.RegisterCommand(F("*IDN?"), &Identify);
  controller.RegisterCommand(F("*RST"), &Reset);

  // MEASure commands
  controller.RegisterCommand(F("MEASure:TICKs?"), &MeasureTicks);
  controller.RegisterCommand(F("MEASure:TICKs:SET"), &SetTicks);
  controller.RegisterCommand(F("MEASure:SENSor:DARK?"), &MeasureDark);
  controller.RegisterCommand(F("MEASure:SENSor:ACTIve?"), &MeasureActive);
  controller.RegisterCommand(F("MEASure:SENSor:EFFEctive?"), &MeasureEffective);
  controller.RegisterCommand(F("MEASure:SENSor:SNR?"), &MeasureSNR);
  controller.RegisterCommand(F("MEASure:TICKs:PERiod?"), &MeasurePeriod);
  controller.RegisterCommand(F("MEASure:TICKs:LAST?"), &MeasureLastTick);

  // CALCulate commands
  controller.RegisterCommand(F("CALCulate:ENERgy?"), &MeasureEnergy);
  controller.RegisterCommand(F("CALCulate:POWer?"), &MeasurePower);

  // CONFigure commands
  controller.RegisterCommand(F("CONFigure:SENSor:THREshold:UPPer"), &ConfSetThreshold);
  controller.RegisterCommand(F("CONFigure:SENSor:THREshold:LOWer"), &ConfSetThreshold);
  controller.RegisterCommand(F("CONFigure:SENSor:THREshold:UPPer?"), &ConfGetThreshold);
  controller.RegisterCommand(F("CONFigure:SENSor:THREshold:LOWer?"), &ConfGetThreshold);
  controller.RegisterCommand(F("CONFigure:SENSor:DURation"), &ConfSetDuration);
  controller.RegisterCommand(F("CONFigure:SENSor:DURation?"), &ConfGetDuration);

  // SYSTem commands
  controller.RegisterCommand(F("SYSTem:CHANnels?"), &CountChannels);
  controller.RegisterCommand(F("SYSTem:FREQuency?"), &GetFrequency);

  reset();

  SERIAL.begin(9600);
  // Wait until serial connection is established
  while (!SERIAL) {;}

  #ifdef ACTIVITY_LED_PIN
    pinMode(ACTIVITY_LED_PIN, OUTPUT);
  #endif

  pinMode(IR_LEDS, OUTPUT);
  for (int i=0; i < NUM_CHANNELS; i++)
    pinMode(IR_SENSORS[i], INPUT);


  // switch off LEDs
  #ifdef LED_BUILTIN_TX
    pinMode(LED_BUILTIN_TX, INPUT);
  #endif
  #ifdef LED_BUILTIN_RX
    pinMode(LED_BUILTIN_RX, INPUT);
  #endif


  #ifdef DEBUG
    delay(2000);
    controller.PrintDebugInfo();
  #endif
}



void loop()
{

  #ifdef ALIGNMENT_MODE
    SERIAL.println(analogRead(ALIGNMENT_PIN));
    delay(100);
  #else
    // Let SCPI parser do its thing
    controller.ProcessInput(SERIAL, "\n");


    time = millis();
    // execute roughly  SAMPLINGRATE-times per second
    if ((time - last_time) >= (500 / SAMPLINGRATE)) {
      last_time = time;
      if (readout_state) {
        for (int i=0; i < NUM_CHANNELS; i++) {
          sensors[i].dark_value = analogRead(IR_SENSORS[i]);  // First step: measure dark current
        }
        digitalWrite(IR_LEDS, HIGH);   // First step: switch on LEDs
        break;
      }
      else {
        for (int i=0; i < NUM_CHANNELS; i++) {
          sensors[i].active_value = analogRead(IR_SENSORS[i]);  // Second step: measure active current and evaluate
          handle_measurement(&sensors[i]);
        }
        livetime_counter++;
        digitalWrite(IR_LEDS, LOW);   // Thirds step: switch off LEDs
        break;
      }
      readout_state = !readout_state;
    }

    // Every second, store and reset livetime_counter
    if ((time - last_time_seconds) >= 1000) {
      last_time_seconds = time;
      livetime_counter_buf = livetime_counter;
      livetime_counter = 0;
    }
  #endif
}
