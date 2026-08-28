#pragma once

#include <Arduino.h>

namespace board {

#if defined(ESP32)
inline constexpr const char *kTargetName = "ESP32-WROOM-32 production";
inline constexpr int kSwdio = 21;
inline constexpr int kSwclk = 18;
inline constexpr int kResetGate = 19; // HIGH turns Q1 on and asserts target NRST.
inline constexpr int kTargetVrefAdc = 34;
inline constexpr int kTargetVrefDigital = -1;
inline constexpr int kFlashButton = 32;
inline constexpr int kWifiLed = 25;
inline constexpr int kFlashingLed = 26;
inline constexpr int kSuccessLed = 27;
inline constexpr int kErrorLed = 33;
inline constexpr bool kConcurrentWebDuringFlash = true;
#elif defined(ESP8266) && defined(BOARD_ESP01_SWD)
// Compact ESP-01/ESP8266 4 MB adapter. GPIO0 drives SWCLK through an
// inverting 2N7002 stage, GPIO1 drives the open-drain NRST stage, and GPIO3
// is a digital VTREF-present input. UART logging must remain disabled.
inline constexpr const char *kTargetName = "ESP8266 ESP-01 4 MB compact";
inline constexpr int kSwdio = 2;
inline constexpr int kSwclk = 0;
inline constexpr int kResetGate = 1;
inline constexpr int kTargetVrefAdc = -1;
inline constexpr int kTargetVrefDigital = 3;
inline constexpr int kFlashButton = -1;
inline constexpr int kWifiLed = -1;
inline constexpr int kFlashingLed = -1;
inline constexpr int kSuccessLed = -1;
inline constexpr int kErrorLed = -1;
inline constexpr bool kConcurrentWebDuringFlash = false;
inline constexpr bool kInvertSwclk = true;
inline constexpr bool kDigitalTargetVref = true;
inline constexpr bool kSerialAvailable = false;
#elif defined(ESP8266)
// NodeMCU prototype wiring: D2, D1, D6, A0, D5, D7 respectively.
inline constexpr const char *kTargetName = "ESP8266 NodeMCU prototype";
inline constexpr int kSwdio = 4;
inline constexpr int kSwclk = 5;
inline constexpr int kResetGate = 12;
inline constexpr int kTargetVrefAdc = A0;
inline constexpr int kTargetVrefDigital = -1;
inline constexpr int kFlashButton = 14;
inline constexpr int kWifiLed = 13;
inline constexpr int kFlashingLed = -1;
inline constexpr int kSuccessLed = -1;
inline constexpr int kErrorLed = -1;
inline constexpr bool kConcurrentWebDuringFlash = false;
#else
#error "This firmware supports ESP32 and ESP8266 Arduino targets only."
#endif

#if !defined(ESP8266) || !defined(BOARD_ESP01_SWD)
inline constexpr bool kInvertSwclk = false;
inline constexpr bool kDigitalTargetVref = false;
inline constexpr bool kSerialAvailable = true;
#endif

inline constexpr uint32_t kMarlinOffset = 0x2000;
inline constexpr uint32_t kFlashBase = 0x08000000;
inline constexpr size_t kMaximumImageSize = 512U * 1024U - kMarlinOffset;
inline constexpr uint32_t kSwdHalfPeriodUs = 1; // About 250 kHz with GPIO overhead.

} // namespace board
