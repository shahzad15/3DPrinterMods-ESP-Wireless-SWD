#include "HardwareAbstraction.hpp"

#include <Arduino.h>

#include "BoardConfig.hpp"

namespace hal {

void initializeSwd() {
  pinMode(board::kSwclk, INPUT);
  pinMode(board::kSwdio, INPUT);
  pinMode(board::kResetGate, OUTPUT);
  digitalWrite(board::kResetGate, LOW);
  if (board::kDigitalTargetVref) pinMode(board::kTargetVrefDigital, INPUT);
}

void connectSwdPins() {
  pinMode(board::kSwclk, OUTPUT);
  writeSwclk(true);
  pinMode(board::kSwdio, OUTPUT);
  digitalWrite(board::kSwdio, HIGH);
}

void disconnectSwdPins() {
  pinMode(board::kSwclk, INPUT);
  pinMode(board::kSwdio, INPUT);
}

void setSwdioOutput(bool output) { pinMode(board::kSwdio, output ? OUTPUT : INPUT); }
void writeSwdio(bool high) { digitalWrite(board::kSwdio, high ? HIGH : LOW); }
bool readSwdio() { return digitalRead(board::kSwdio) != LOW; }
void writeSwclk(bool high) {
  const bool pinHigh = board::kInvertSwclk ? !high : high;
  digitalWrite(board::kSwclk, pinHigh ? HIGH : LOW);
}
void setTargetReset(bool asserted) { digitalWrite(board::kResetGate, asserted ? HIGH : LOW); }

float readTargetVoltage() {
  if (board::kDigitalTargetVref) {
    // The ESP-01 lacks a free ADC pin. A 47k/330k divider makes GPIO3 a
    // conservative target-power-present detector, not a voltmeter.
    return digitalRead(board::kTargetVrefDigital) == HIGH ? 3.3f : 0.0f;
  }
#if defined(ESP32)
  return (analogRead(board::kTargetVrefAdc) / 4095.0f) * 3.3f * 2.0f;
#else
  return (analogRead(board::kTargetVrefAdc) / 1023.0f) * 3.3f;
#endif
}

void swdHalfPeriodDelay() { delayMicroseconds(board::kSwdHalfPeriodUs); }

} // namespace hal
