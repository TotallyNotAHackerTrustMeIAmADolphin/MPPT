#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "Arduino.h"

// Pin definitions
constexpr uint8_t PIN_LED = PC10;
constexpr uint8_t VSENSE_IN_PIN = PA1;
constexpr uint8_t VSENSE_OUT_PIN = PA3;
constexpr uint8_t ISENSE_IN_PIN = PA2;
constexpr uint8_t ISENSE_OUT_PIN = PA4;

constexpr uint8_t PIN_Q5 = PA9;
constexpr uint8_t PIN_Q8 = PB0;
constexpr uint8_t PIN_Q6 = PA8;
constexpr uint8_t PIN_Q7 = PA7;

// conversion factor for converting ADC values to voltage with 200k and 5.1k resistors at 12 bit resolution of 3.3V
constexpr float RESISTOR1 = 200.0f;
constexpr float RESISTOR2 = 4.7f;
constexpr float VOLTAGE_CONVERSION_FACTOR = (3.3f / 4095.0f) * ((RESISTOR1 + RESISTOR2) / RESISTOR2);

// conversion factor from ADC values to current in mA with 0.333333 milli-ohm resistor at 12 bit resolution of 3.3V where 1.65V is the zero current point at 200 V/V gain
constexpr float shunt_resistor = 0.000333333333333333f;
constexpr float CURRENT_CONVERSION_FACTOR = -(3.3f / 4095.0f) / 200 / shunt_resistor * 1000.0f;


#endif // CONSTANTS_H