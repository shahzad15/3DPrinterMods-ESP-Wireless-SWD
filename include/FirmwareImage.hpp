#pragma once

#include <Arduino.h>
#include <FS.h>

struct FirmwareMetadata {
  bool marlinDetected{false};
  String firmwareName;
  String machineName;
  String boardName;
  String mcuName;
  String sourceCodeUrl;
  String protocolVersion;
  String extruderCount;
  String uuid;
};

class FirmwareMetadataScanner {
public:
  void feed(const uint8_t *data, size_t size);
  void finish();
  const FirmwareMetadata &metadata() const { return metadata_; }

private:
  void consumeRun();

  static constexpr size_t kRunCapacity = 384;
  char run_[kRunCapacity + 1]{};
  size_t runLength_{0};
  FirmwareMetadata metadata_;
};

struct ImageInfo {
  bool valid{false};
  size_t size{0};
  uint32_t loadAddress{0};
  uint32_t initialSp{0};
  uint32_t resetVector{0};
  uint32_t crc32{0};
  FirmwareMetadata metadata;
  String error;
};

uint32_t updateCrc32(uint32_t crc, const uint8_t *data, size_t size);

ImageInfo inspectMarlinImage(fs::FS &fs, const char *path, uint32_t loadAddress,
                             size_t maximumSize);
