/*
  laser.cpp - Laser control library for Arduino using 16 bit timers- Version 1
  Copyright (c) 2013 Timothy Schmidt.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "laser.h"
#include "Configuration.h"
#include "ConfigurationStore.h"
#include "pins.h"
#include <avr/interrupt.h>
#include <Arduino.h>
#include "Marlin.h"

laser_t laser;

void timer3_init(int pin) {

  digitalWrite(pin, HIGH);
  pinMode(pin, OUTPUT);

  // Setup timer3 to fast PWM with OC w/ ICR3 as TOP
  noInterrupts();

  TCCR3A = 0x00;  
  TCCR3B = 0x00;  // stop Timer3 clock for register updates  
  ICR3 = labs(F_CPU / LASER_PWM); // set clock cycles per PWM pulse (OC Top value)  

  if (pin == 2) {
    TCCR3A = _BV(COM3B1) | _BV(COM3B0) | _BV(WGM31); // Fast PWM (WGM31) / (Clear OC3B/pin 2 on compare match (set output to low level)
    OCR3B = 0;
  }

  if (pin == 3) {
    TCCR3A = _BV(COM3C1) | _BV(COM3C0) | _BV(WGM31); // Fast PWM (WGM31) / Clear OC3C/pin 3 on compare match (set output to low level)
    OCR3C = 0;
  }

  if (pin == 5) {
    TCCR3A = _BV(COM3A1) | _BV(COM3A0) | _BV(WGM31); // Fast PWM (WGM31) / Clear OC3A/pin 5 on compare match (set output to low level)
    OCR3A = 0;
  }

  TCCR3B = _BV(CS30) | _BV(WGM33) |  _BV(WGM32); // Fast PWM / clkIo/1 (No prescaling) 

  interrupts();
}

void timer4_init(int pin) {

  digitalWrite(pin, HIGH);
  pinMode(pin, OUTPUT);

  // Setup timer4 to fast PWM with OC w/ ICR4 as TOP
  noInterrupts();

  TCCR4A = 0x00;  
  TCCR4B = 0x00;  // stop Timer4 clock for register updates
  TCCR4C = 0x00;
  
  ICR4 = labs(F_CPU / LASER_PWM); // set clock cycles per PWM pulse (OC Top value)  

  if (pin == 6) {
    TCCR4A = _BV(COM4A1) |  _BV(COM4A0) | _BV(WGM41); // Fast PWM (WGM41) / Clear OC4A/pin 5 on compare match (set output to low level)
    OCR4A = 0;
  }

  if (pin == 7) {
    TCCR4A = _BV(COM4B1) | _BV(COM4B0) | _BV(WGM41); // Fast PWM (WGM41) / (Clear OC4B/pin 2 on compare match (set output to low level)
    OCR4B = 0;
  }

  if (pin == 8) {
    TCCR4A = _BV(COM4C1) | _BV(COM4C0) | _BV(WGM41); // Fast PWM (WGM41) / Clear OC4C/pin 4 on compare match (set output to low level)
    OCR4C = labs(F_CPU / LASER_PWM); // Set OCR4C to TOP value so it doesnt compare
  }

  TCCR4B = _BV(CS40) | _BV(WGM43) |  _BV(WGM42); // Fast PWM / clkIo/1 (No prescaling) 

  interrupts();
  
}

void laser_init()
{
  // Initialize timers for laser intensity control
  #if LASER_CONTROL == 1
    if (LASER_FIRING_PIN == 2 || LASER_FIRING_PIN == 3 || LASER_FIRING_PIN == 5) timer3_init(LASER_FIRING_PIN);
    if (LASER_FIRING_PIN == 6 || LASER_FIRING_PIN == 7 || LASER_FIRING_PIN == 8) timer4_init(LASER_FIRING_PIN);
  #endif
  #if LASER_CONTROL == 2
    if (LASER_INTENSITY_PIN == 2 || LASER_INTENSITY_PIN == 3 || LASER_INTENSITY_PIN == 5) timer3_init(LASER_INTENSITY_PIN);
    if (LASER_INTENSITY_PIN == 6 || LASER_INTENSITY_PIN == 7 || LASER_INTENSITY_PIN == 8) timer4_init(LASER_INTENSITY_PIN);
  #endif

  #ifdef LASER_PERIPHERALS
    digitalWrite(LASER_COOLANT, HIGH);  // Laser peripherals are active LOW, so preset the pin
    pinMode(LASER_COOLANT, OUTPUT);

    digitalWrite(LASER_AIR, HIGH);
    pinMode(LASER_AIR, OUTPUT);

    digitalWrite(LASER_POWER, HIGH);
    pinMode(LASER_POWER, OUTPUT);

    digitalWrite(LASER_EXHAUST, HIGH);
    pinMode(LASER_EXHAUST, OUTPUT);
  #endif // LASER_PERIPHERALS

  // initialize state to some sane defaults
  laser.intensity = 100.0;
  laser.ppm = 0.0;
  laser.duration = 0;
  laser.status = LASER_OFF;
  laser.firing = LASER_OFF;
  laser.mode = CONTINUOUS;
  laser.last_firing = 0;
  laser.diagnostics = false;
  laser.time = 0;
  #ifdef LASER_RASTER
    laser.raster_aspect_ratio = LASER_RASTER_ASPECT_RATIO;
    laser.raster_mm_per_pulse = LASER_RASTER_MM_PER_PULSE;
    laser.raster_direction = 1;
  #endif // LASER_RASTER
  #ifdef MUVE_Z_PEEL
    laser.peel_distance = 2.0;
    laser.peel_speed = 2.0;
    laser.peel_pause = 0.0;
  #endif // MUVE_Z_PEEL

  laser_extinguish();
}
void laser_fire(int intensity = 100.0) {

    laser.firing = LASER_ON;
    laser.last_firing = micros(); // microseconds of last laser firing
	
    if (intensity > 100.0) intensity = 100.0; // restrict intensity between 0 and 100
    if (intensity < 0) intensity = 0;

    #if LASER_CONTROL == 1
      #if LASER_FIRING_PIN == 2
        OCR3B = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_FIRING_PIN == 3
        OCR3C = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_FIRING_PIN == 5
        OCR3A = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif
      #if LASER_FIRING_PIN == 6
        OCR4A = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_FIRING_PIN == 7
        OCR4B = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_FIRING_PIN == 8
        OCR4C = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif
    #endif

    #if LASER_CONTROL == 2
      #if LASER_INTENSITY_PIN == 2
        OCR3B = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_INTENSITY_PIN == 3
        OCR3C = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_INTENSITY_PIN == 5
        OCR3A = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif
      #if LASER_INTENSITY_PIN == 6
        OCR4A = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_INTENSITY_PIN == 7
        OCR4B = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif    
      #if LASER_INTENSITY_PIN == 8
        OCR4C = labs((intensity / 100.0)*(F_CPU / LASER_PWM));
      #endif
      digitalWrite(LASER_FIRING_PIN, LOW);
    #endif

    if (laser.diagnostics) {
		SERIAL_ECHOLN("Laser fired");
    }
}

void laser_extinguish(){
  if (laser.firing == LASER_ON) {
    laser.firing = LASER_OFF;

    #if LASER_CONTROL == 1
      #if LASER_FIRING_PIN == 2
        OCR3B = 0;
      #endif    
      #if LASER_FIRING_PIN == 3
        OCR3C = 0;
      #endif    
      #if LASER_FIRING_PIN == 5
        OCR3A = 0;
      #endif
      #if LASER_FIRING_PIN == 6
        OCR4A = 0;
      #endif    
      #if LASER_FIRING_PIN == 7
        OCR4B = 0;
      #endif    
      #if LASER_FIRING_PIN == 8
        OCR4C = 0;
      #endif
    #endif

    #if LASER_CONTROL == 2
      #if LASER_INTENSITY_PIN == 2
        OCR3B = 0;
      #endif    
      #if LASER_INTENSITY_PIN == 3
        OCR3C = 0;
      #endif    
      #if LASER_INTENSITY_PIN == 5
        OCR3A = 0;
      #endif
      #if LASER_INTENSITY_PIN == 6
        OCR4A = 0;
      #endif    
      #if LASER_INTENSITY_PIN == 7
        OCR4B = 0;
      #endif    
      #if LASER_INTENSITY_PIN == 8
        OCR4C = 0;
      #endif
      digitalWrite(LASER_FIRING_PIN, HIGH);
    #endif
    
    laser.time += millis() - (laser.last_firing / 1000);

    if (laser.diagnostics) {
      SERIAL_ECHOLN("Laser extinguished");
    }
  }
}
void laser_set_mode(int mode){
  switch(mode){
    case 0:
      laser.mode = CONTINUOUS;
      return;
    case 1:
      laser.mode = PULSED;
      return;
    case 2:
      laser.mode = RASTER;
      return;
  }
}
#ifdef LASER_PERIPHERALS
bool laser_peripherals_ok(){
  return true;
}
void laser_peripherals_on(){
  digitalWrite(LASER_COOLANT, LOW);
  digitalWrite(LASER_AIR, LOW);
  digitalWrite(LASER_POWER, LOW);
  digitalWrite(LASER_EXHAUST, LOW);
  fanSpeed=255;

  if (laser.diagnostics) {
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Laser Peripherals Enabled");
    }
}
void laser_peripherals_off(){
  if (laser_peripherals_ok()) {
    digitalWrite(LASER_COOLANT, HIGH);
    digitalWrite(LASER_AIR, HIGH);
    digitalWrite(LASER_POWER, HIGH);
    digitalWrite(LASER_EXHAUST, HIGH);
    fanSpeed=0;

    if (laser.diagnostics) {
      SERIAL_ECHO_START;
      SERIAL_ECHOLNPGM("Laser Peripherals Disabled");
      }
    }
}
void laser_wait_for_peripherals() {
  unsigned long timeout = millis() + LASER_PERIPHERALS_TIMEOUT;
  if (laser.diagnostics) {
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Waiting for peripheral control board signal...");
  }
  while(!laser_peripherals_ok()) {
    if (millis() > timeout) {
      if (laser.diagnostics) {
        SERIAL_ERROR_START;
        SERIAL_ERRORLNPGM("Peripheral control board failed to respond");
      }
      Stop();
      break;
    }
  }
}
#endif // LASER_PERIPHERALS
