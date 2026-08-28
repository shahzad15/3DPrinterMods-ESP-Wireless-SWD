#pragma once

#include <Arduino.h>
#include <FS.h>

#include "FirmwareImage.hpp"
#include "SwdTransport.hpp"

struct TargetInfo {
  uint32_t debugPortId{0};
  uint32_t deviceIdCode{0};
  uint16_t deviceId{0};
  uint16_t revisionId{0};
  uint32_t flashBytes{0};
  uint32_t uniqueId[3]{};
  String model;
  String uniqueIdText;
};

struct ApplicationInspection {
  bool identicalToUpload{false};
  uint32_t initialSp{0};
  uint32_t resetVector{0};
  uint32_t crc32{0};
  FirmwareMetadata metadata;
};

using ProgressCallback = void (*)(uint8_t percent, const String &message);

class Stm32G0Programmer {
public:
  explicit Stm32G0Programmer(SwdTransport &swd) : swd_(swd) {}

  bool attach(TargetInfo &info);
  void detachAndRun();
  bool inspectApplication(fs::FS &fs, const char *uploadPath, uint32_t offset,
                          size_t size, ApplicationInspection &inspection,
                          ProgressCallback progress);
  bool backup(fs::FS &fs, const char *path, uint32_t offset, size_t size,
              ProgressCallback progress);
  bool program(fs::FS &fs, const char *path, uint32_t offset, size_t size,
               ProgressCallback progress);
  bool verify(fs::FS &fs, const char *path, uint32_t offset, size_t size,
              ProgressCallback progress);
  const String &lastError() const { return error_; }

private:
  bool unlockFlash();
  bool waitFlash(uint32_t timeoutMs);
  bool erasePage(uint32_t page);
  bool programPage(File &file, uint32_t address, size_t bytesFromFile);
  bool check(bool result, const String &context);

  SwdTransport &swd_;
  String error_;
};
