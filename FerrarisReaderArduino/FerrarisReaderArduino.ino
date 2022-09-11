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

//#define DEBUG

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

uint16_t active_value[NUM_CHANNELS];
uint16_t dark_value[NUM_CHANNELS];

int high_counter[NUM_CHANNELS];
int low_counter[NUM_CHANNELS];
bool state[NUM_CHANNELS];
unsigned long ticks[NUM_CHANNELS];


int last_tick[NUM_CHANNELS];
int second_last_tick[NUM_CHANNELS];


int get_channel_number(SCPI_P parameters) {
  if (parameters.Size() == 0)
    return 0;
  int temp = String(parameters[0]).toInt();
  if ((temp >= 0) && (temp < NUM_CHANNELS))
    return temp;
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
//                           SCPI standard commands                           //
////////////////////////////////////////////////////////////////////////////////



void Identify(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(F("OTTO, FerrarisReader, V1.0, V1.0"));
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}


// Resets the Controller Board
void Reset(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  for (int i=0; i<NUM_CHANNELS; i++) {
    ticks[i] = 0;
    high_counter[i] = 0;
    low_counter[i] = 0;
    state[i] = true;
  }
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureTicks(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(ticks[get_channel_number(parameters)]);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureDark(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(dark_value[get_channel_number(parameters)]);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureActive(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(active_value[get_channel_number(parameters)]);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureEffective(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  int channel_number = get_channel_number(parameters);
  interface.println(active_value[channel_number] - dark_value[channel_number]);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureEnergy(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(((float) ticks[get_channel_number(parameters)]) / TURNS_PER_KWH);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasurePower(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  int channel_number = get_channel_number(parameters);
  if (ticks[channel_number] < 2)
    interface.println(F("Invalid"));
  else {
    interface.println(KWH_PER_TURN * 3600 / (last_tick[channel_number] - second_last_tick[channel_number]), 4);
  }
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasurePeriod(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  int channel_number = get_channel_number(parameters);
  if (ticks[channel_number] < 2)
    interface.println(F("Invalid"));
  else {
    interface.println(last_tick[channel_number] - second_last_tick[channel_number]);
  }
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureLastTick(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  int channel_number = get_channel_number(parameters);
  if (ticks[channel_number] < 1)
    interface.println(F("Invalid"));
  else
    interface.println((millis() / 1000) - last_tick[channel_number]);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void tick(int index) {
  ticks[index]++;
  second_last_tick[index] = last_tick[index];
  last_tick[index] = millis() / 1000;
}


void calculate_everything(uint16_t active_value, uint16_t dark_value, int index) {
  int difference = active_value - dark_value;

  if (difference > UPPER_THRESHOLD) {
    high_counter[index]++;
    if (high_counter[index] > MIN_DURATION)
      state[index] = true;
  }
  else {
    high_counter[index] = 0;
    if (difference < LOWER_THRESHOLD) {
      low_counter[index]++;
      if (low_counter[index] > MIN_DURATION) {
        if (state[index])
          tick(index);
        state[index] = false;
      }
    }
    else {
      low_counter[index] = 0;
    }
  }
}



void setup() {
  // SCPI standard commands
  controller.RegisterCommand(F("*IDN?"), &Identify);
  controller.RegisterCommand(F("*RST"), &Reset);

  // MEASure commands
  controller.RegisterCommand(F("MEASure:TICKs?"), &MeasureTicks);
  controller.RegisterCommand(F("MEASure:TICKs:NUMber?"), &MeasureTicks);
  controller.RegisterCommand(F("MEASure:SENSor:DARK?"), &MeasureDark);
  controller.RegisterCommand(F("MEASure:SENSor:ACTIve?"), &MeasureActive);
  controller.RegisterCommand(F("MEASure:SENSor:EFFEctive?"), &MeasureEffective);
  controller.RegisterCommand(F("MEASure:TICK:PERiod?"), &MeasurePeriod);
  controller.RegisterCommand(F("MEASure:TICK:LAST?"), &MeasureLastTick);
  controller.RegisterCommand(F("CALCulate:ENERgy?"), &MeasureEnergy);
  controller.RegisterCommand(F("CALCulate:POWer?"), &MeasurePower);

  for (int i=0; i < NUM_CHANNELS; i++) {
    ticks[i] = 0;
    high_counter[i] = 0;
    low_counter[i] = 0;
    state[i] = true;
  }

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
        active_value[i] = analogRead(IR_SENSORS[i]);   // Second step: measure active current
      }
      break;
      }
    case 2: {
      digitalWrite(IR_LEDS, LOW);   // Thirds step: switch off LEDs
      break;
      }
    case 3: {
      for (int i=0; i < NUM_CHANNELS; i++) {
        dark_value[i] = analogRead(IR_SENSORS[i]);   // Fourth step: measure dark current and evaluate
        calculate_everything(active_value[i], dark_value[i], i);
      }
      break;
      }
    }
    counter = (counter + 1) & 3;
  }
}