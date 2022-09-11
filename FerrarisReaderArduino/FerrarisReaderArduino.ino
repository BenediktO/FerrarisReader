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

#include "Vrekrer_scpi_parser.h"

#define DEBUG

SCPI_Parser controller;



#define NUM_CHANNELS 2


#define ACTIVITY_LED_PIN 10

#define UPPER_THRESHOLD 100
#define LOWER_THRESHOLD 50
#define MIN_DURATION 20


#define TURNS_PER_KWH 75
#define KWH_PER_TURN (1.0 / TURNS_PER_KWH)


int IR_SENSORS[NUM_CHANNELS] = {A0, A1};
int IR_LEDS = 9;  // common pin


unsigned long last_time;
int counter = 0;


struct SensorData
{
  uint16_t active_value;
  uint16_t dark_value;
  int high_counter;
  int low_counter;
  bool state;
  uint32_t ticks;
  int last_tick;
  int second_last_tick;
};


struct SensorData sensors[NUM_CHANNELS];


int get_channel_number(SCPI_P parameters) {
  if (parameters.Size() < 1)
    return 0;
  int temp = String(parameters[0]).toInt();
  if ((0 <= temp) && (temp < NUM_CHANNELS))
    return temp;
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//                           SCPI standard commands                           //
////////////////////////////////////////////////////////////////////////////////



void reset(void) {
  struct SensorData sensor;
  for (int i=0; i<NUM_CHANNELS; i++) {
    sensor = sensors[i];
    sensor.ticks = 0;
    sensor.high_counter = 0;
    sensor.low_counter = 0;
    sensor.state = true;
  }
}


void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(F("OTTO, FerrarisReader, V1.0, V1.0"));
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}


// Resets the Controller Board
void Reset(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  reset();
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureTicks(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.ticks);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void SetTicks(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  if (parameters.Size() >= 2) {
    struct SensorData * sensor = &sensors[get_channel_number(parameters)];
    sensor->ticks = String(parameters[1]).toInt();
  }
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureDark(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.dark_value);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureActive(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.active_value);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureEffective(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(sensor.active_value - sensor.dark_value);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureEnergy(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  interface.println(((float) sensor.ticks) / TURNS_PER_KWH);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasurePower(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  if (sensor.ticks < 2)
    interface.println(F("Invalid"));
  else {
    interface.println(KWH_PER_TURN * 3600 / (sensor.last_tick - sensor.second_last_tick), 4);
  }
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasurePeriod(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  if (sensor.ticks < 2)
    interface.println(F("Invalid"));
  else {
    interface.println(sensor.last_tick - sensor.second_last_tick);
  }
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureLastTick(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  struct SensorData sensor = sensors[get_channel_number(parameters)];
  if (sensor.ticks < 1)
    interface.println(F("Invalid"));
  else
    interface.println((millis() / 1000) - sensor.last_tick);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void tick(struct SensorData * sensor_data) {
  sensor_data->ticks++;
  sensor_data->second_last_tick = sensor_data->last_tick;
  sensor_data->last_tick = millis() / 1000;
}


void calculate_everything(struct SensorData * sensor_data) {
  int difference = sensor_data->active_value - sensor_data->dark_value;

  if (difference > UPPER_THRESHOLD) {
    sensor_data->high_counter++;
    if (sensor_data->high_counter > MIN_DURATION)
      sensor_data->state = true;
  }
  else {
    sensor_data->high_counter = 0;
    if (difference < LOWER_THRESHOLD) {
      sensor_data->low_counter++;
      if (sensor_data->low_counter > MIN_DURATION) {
        if (sensor_data->state)
          tick(sensor_data);
        sensor_data->state = false;
      }
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
  controller.RegisterCommand(F("MEASure:TICK:PERiod?"), &MeasurePeriod);
  controller.RegisterCommand(F("MEASure:TICK:LAST?"), &MeasureLastTick);
  controller.RegisterCommand(F("CALCulate:ENERgy?"), &MeasureEnergy);
  controller.RegisterCommand(F("CALCulate:POWer?"), &MeasurePower);

  reset();

  Serial.begin(9600);
  // Wait until serial connection is established
  while (!Serial) {;}

  pinMode(ACTIVITY_LED_PIN, OUTPUT);
  pinMode(IR_LEDS, OUTPUT);
  for (int i=0; i < NUM_CHANNELS; i++)
    pinMode(IR_SENSORS[i], INPUT);


  #ifdef DEBUG
    delay(2000);
    controller.PrintDebugInfo();
  #endif
}



void loop()
{
  controller.ProcessInput(Serial, "\n");

  if ((millis() - last_time) > 5) {
    last_time = millis();
    switch (counter) {
    case 0: {
      digitalWrite(IR_LEDS, HIGH);   // First step: switch on LEDs
      break;
      }
    case 1: {
      for (int i=0; i < NUM_CHANNELS; i++) {
        sensors[i].active_value = analogRead(IR_SENSORS[i]);  // Second step: measure active current
      }
      break;
      }
    case 2: {
      digitalWrite(IR_LEDS, LOW);   // Thirds step: switch off LEDs
      break;
      }
    case 3: {
      for (int i=0; i < NUM_CHANNELS; i++) {
        sensors[i].dark_value = analogRead(IR_SENSORS[i]);  // Fourth step: measure dark current and evaluate
        calculate_everything(&sensors[i]);
      }
      break;
      }
    }
    counter = (counter + 1) & 3;
  }
}
