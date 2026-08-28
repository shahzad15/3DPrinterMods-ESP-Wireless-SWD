#include "FirmwareImage.hpp"

namespace {
uint32_t littleEndian32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8U) |
         (static_cast<uint32_t>(p[2]) << 16U) |
         (static_cast<uint32_t>(p[3]) << 24U);
}

String fieldValue(const String &text, const String &upper, const char *field,
                  const char *const *terminators, size_t terminatorCount) {
  const int startAt = upper.indexOf(field);
  if (startAt < 0) return "";
  const int valueAt = startAt + strlen(field);
  int endAt = text.length();
  for (size_t i = 0; i < terminatorCount; ++i) {
    const int candidate = upper.indexOf(terminators[i], valueAt);
    if (candidate >= 0 && candidate < endAt) endAt = candidate;
  }
  String value = text.substring(valueAt, endAt);
  value.trim();
  if (value.length() > 96) value.remove(96);
  return value;
}

String explicitBoardName(const String &upper) {
  if (upper.indexOf("SKR MINI E3 V3") >= 0 ||
      upper.indexOf("BTT_SKR_MINI_E3_V3") >= 0 ||
      upper.indexOf("BOARD_BTT_SKR_MINI_E3_V3_0") >= 0)
    return "BTT SKR Mini E3 V3.0";
  if (upper.indexOf("SKR MINI E3 V2") >= 0 ||
      upper.indexOf("BTT_SKR_MINI_E3_V2") >= 0)
    return "BTT SKR Mini E3 V2.0";
  if (upper.indexOf("SKR MINI E3 V1") >= 0 ||
      upper.indexOf("BTT_SKR_MINI_E3_V1") >= 0)
    return "BTT SKR Mini E3 V1.x";
  if (upper.indexOf("SKR PRO") >= 0) return "BTT SKR Pro";
  if (upper.indexOf("BTT SKR V3") >= 0 || upper.indexOf("BTT_SKR_V3") >= 0)
    return "BTT SKR V3.0";
  return "";
}

String explicitMcuName(const String &upper) {
  if (upper.indexOf("STM32G0B1") >= 0) return "STM32G0B1";
  if (upper.indexOf("STM32G0C1") >= 0) return "STM32G0C1";
  if (upper.indexOf("STM32G0B0") >= 0) return "STM32G0B0";
  if (upper.indexOf("STM32F103") >= 0) return "STM32F103";
  if (upper.indexOf("STM32F401") >= 0) return "STM32F401";
  if (upper.indexOf("STM32F407") >= 0) return "STM32F407";
  if (upper.indexOf("STM32H743") >= 0) return "STM32H743";
  return "";
}
} // namespace

uint32_t updateCrc32(uint32_t crc, const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return crc;
}

void FirmwareMetadataScanner::consumeRun() {
  if (runLength_ < 4) { runLength_ = 0; return; }
  run_[runLength_] = '\0';
  const String text(run_);
  String upper(text);
  upper.toUpperCase();

  if (upper.indexOf("MARLIN") >= 0 || upper.indexOf("FIRMWARE_NAME:") >= 0)
    metadata_.marlinDetected = true;

  static const char *const firmwareEnds[] = {
      " SOURCE_CODE_URL:", " PROTOCOL_VERSION:", " MACHINE_TYPE:"};
  if (metadata_.firmwareName.length() == 0) {
    metadata_.firmwareName = fieldValue(text, upper, "FIRMWARE_NAME:",
        firmwareEnds, sizeof(firmwareEnds) / sizeof(firmwareEnds[0]));
  }

  static const char *const machineEnds[] = {
      " EXTRUDER_COUNT:", " UUID:", " CAP:"};
  if (metadata_.machineName.length() == 0) {
    metadata_.machineName = fieldValue(text, upper, "MACHINE_TYPE:",
        machineEnds, sizeof(machineEnds) / sizeof(machineEnds[0]));
  }

  if (metadata_.boardName.length() == 0) metadata_.boardName = explicitBoardName(upper);
  if (metadata_.mcuName.length() == 0) metadata_.mcuName = explicitMcuName(upper);

  static const char *const sourceEnds[] = {" PROTOCOL_VERSION:", " MACHINE_TYPE:"};
  if (metadata_.sourceCodeUrl.length() == 0) {
    metadata_.sourceCodeUrl = fieldValue(text, upper, "SOURCE_CODE_URL:",
        sourceEnds, sizeof(sourceEnds) / sizeof(sourceEnds[0]));
  }

  static const char *const protocolEnds[] = {" MACHINE_TYPE:", " EXTRUDER_COUNT:"};
  if (metadata_.protocolVersion.length() == 0) {
    metadata_.protocolVersion = fieldValue(text, upper, "PROTOCOL_VERSION:",
        protocolEnds, sizeof(protocolEnds) / sizeof(protocolEnds[0]));
  }

  static const char *const extruderEnds[] = {" UUID:", " CAP:"};
  if (metadata_.extruderCount.length() == 0) {
    metadata_.extruderCount = fieldValue(text, upper, "EXTRUDER_COUNT:",
        extruderEnds, sizeof(extruderEnds) / sizeof(extruderEnds[0]));
  }

  static const char *const uuidEnds[] = {" CAP:"};
  if (metadata_.uuid.length() == 0) {
    metadata_.uuid = fieldValue(text, upper, "UUID:",
        uuidEnds, sizeof(uuidEnds) / sizeof(uuidEnds[0]));
  }
  runLength_ = 0;
}

void FirmwareMetadataScanner::feed(const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    const uint8_t c = data[i];
    if (c >= 0x20 && c <= 0x7e) {
      if (runLength_ == kRunCapacity) consumeRun();
      run_[runLength_++] = static_cast<char>(c);
    } else {
      consumeRun();
    }
  }
}

void FirmwareMetadataScanner::finish() { consumeRun(); }

ImageInfo inspectMarlinImage(fs::FS &fs, const char *path, uint32_t loadAddress,
                             size_t maximumSize) {
  ImageInfo info;
  File file = fs.open(path, "r");
  if (!file) {
    info.error = "Uploaded file is missing";
    return info;
  }
  info.size = file.size();
  info.loadAddress = loadAddress;
  if (info.size < 256 || info.size > maximumSize) {
    info.error = "Image size is outside the safe Marlin application range";
    return info;
  }
  uint8_t vectors[8]{};
  if (file.read(vectors, sizeof(vectors)) != sizeof(vectors)) {
    info.error = "Could not read the vector table";
    return info;
  }
  info.initialSp = littleEndian32(vectors);
  info.resetVector = littleEndian32(vectors + 4);

  if (!file.seek(0)) {
    info.error = "Could not rewind uploaded image";
    return info;
  }
  FirmwareMetadataScanner scanner;
  uint32_t crc = 0xffffffffU;
  uint8_t scanBuffer[256];
  while (file.available()) {
    const size_t count = file.read(scanBuffer, sizeof(scanBuffer));
    if (count == 0) break;
    scanner.feed(scanBuffer, count);
    crc = updateCrc32(crc, scanBuffer, count);
    delay(0);
  }
  scanner.finish();
  info.crc32 = ~crc;
  info.metadata = scanner.metadata();

  // STM32G0B1 has SRAM at 0x20000000 and up to 144 KiB. The reset handler
  // must be Thumb code inside the application image's possible flash range.
  if (info.initialSp < 0x20000000U || info.initialSp > 0x20024000U ||
      (info.initialSp & 3U) != 0U) {
    info.error = "Invalid STM32G0 stack pointer in vector table";
    return info;
  }
  const uint32_t handler = info.resetVector & ~1U;
  if ((info.resetVector & 1U) == 0U || handler < loadAddress ||
      handler >= loadAddress + maximumSize) {
    info.error = "Reset vector is not linked for 0x08002000";
    return info;
  }
  info.valid = true;
  return info;
}
