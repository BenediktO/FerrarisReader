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


#define ACTIVITY_LED_PIN 10
#define IR_SENSOR A0
#define IR_LED 9



#define UPPER_THRESHOLD 50
#define LOWER_THRESHOLD 20
#define MIN_DURATION 20


#define TURNS_PER_KWH 75
#define KWH_PER_TURN (1.0 / TURNS_PER_KWH)



unsigned long last_time;
int counter = 0;

uint16_t active_value;
uint16_t dark_value;

int high_counter = 0;
int low_counter = 0;
bool state = true;
unsigned long ticks;


int last_tick;
int second_last_tick;


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
  ticks = 0;
  high_counter = 0;
  low_counter = 0;
  state = true;
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureTicks(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(ticks);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureDark(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(dark_value);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureActive(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(active_value);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureEffective(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(active_value - dark_value);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureEnergy(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  interface.println(((float) ticks) / TURNS_PER_KWH, 4);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasurePower(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  if (ticks < 2)
    interface.println(F("Invalid"));
  else
    interface.println(KWH_PER_TURN * 3600 / (last_tick - second_last_tick), 4);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasurePeriod(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  if (ticks < 2)
    interface.println(F("Invalid"));
  else
    interface.println(last_tick - second_last_tick);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void MeasureLastTick(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  digitalWrite(ACTIVITY_LED_PIN, HIGH);
  if (ticks < 1)
    interface.println(F("Invalid"));
  else
    interface.println((millis() / 1000) - last_tick);
  digitalWrite(ACTIVITY_LED_PIN, LOW);
}

void tick() {
  ticks++;
  second_last_tick = last_tick;
  last_tick = millis() / 1000;
}


void calculate_everything(uint16_t active_value, uint16_t dark_value) {
  int difference = active_value - dark_value;

  if (difference > UPPER_THRESHOLD) {
    high_counter++;
    if (high_counter > MIN_DURATION)
      state = true;
  }
  else {
    high_counter = 0;
    if (difference < LOWER_THRESHOLD) {
      low_counter++;
      if (low_counter > MIN_DURATION) {
        if (state)
          tick();
        state = false;
      }
    }
    else {
      low_counter = 0;
    }
  }
}



void setup() {
  // SCPI standard commands
  controller.RegisterCommand(F("*IDN?"), &Identify);
  controller.RegisterCommand(F("*RST?"), &Reset);

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

  Serial.begin(9600);
  // Wait until serial connection is established
  while (!Serial) {;}

  pinMode(ACTIVITY_LED_PIN, OUTPUT);
  pinMode(IR_LED, OUTPUT);


  #ifdef DEBUG
    delay(2000);
    controller.PrintDebugInfo();
  #endif
}



void loop()
{
  controller.ProcessInput(Serial, "\n");


  // if (Serial.available()) {
  //   Serial.println(Serial.read());
  // }


  if ((millis() - last_time) > 5) {
    last_time = millis();
    switch (counter) {
    case 0: {
      digitalWrite(IR_LED, HIGH);   // First step: switch on LED
      break;
      }
    case 1: {
      active_value = analogRead(IR_SENSOR);   // Second step: measure active current
      break;
      }
    case 2: {
      digitalWrite(IR_LED, LOW);   // Third step: switch off LED
      break;
      }
    case 3: {
      dark_value = analogRead(IR_SENSOR);   // Fourth step: measure dark current and evaluate
      calculate_everything(active_value, dark_value);
      break;
      }
    }
    counter = (counter + 1) & 3;
  }
}
