#pragma once

namespace hal {

void initializeSwd();
void connectSwdPins();
void disconnectSwdPins();
void setSwdioOutput(bool output);
void writeSwdio(bool high);
bool readSwdio();
void writeSwclk(bool high);
void setTargetReset(bool asserted);
float readTargetVoltage();
void swdHalfPeriodDelay();

} // namespace hal

