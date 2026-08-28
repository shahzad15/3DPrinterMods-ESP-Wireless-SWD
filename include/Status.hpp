#pragma once

#include <Arduino.h>

enum class JobState {
  Idle,
  Uploading,
  Probing,
  Ready,
  Current,
  Connecting,
  BackingUp,
  Erasing,
  Programming,
  Verifying,
  Restoring,
  Success,
  Error,
};

struct JobStatus {
  JobState state{JobState::Idle};
  uint8_t progress{0};
  size_t imageSize{0};
  uint32_t imageLoadAddress{0};
  uint32_t uploadedInitialSp{0};
  uint32_t uploadedResetVector{0};
  uint32_t uploadedCrc32{0};
  uint32_t installedInitialSp{0};
  uint32_t installedResetVector{0};
  uint32_t installedCrc32{0};
  uint32_t targetDebugPortId{0};
  uint32_t targetId{0};
  uint32_t targetIdCode{0};
  uint16_t targetRevision{0};
  uint32_t flashBytes{0};
  String targetModel;
  String targetUniqueId;
  String uploadedFirmware;
  String uploadedMachine;
  String uploadedBoard;
  String uploadedMcu;
  String uploadedSourceUrl;
  String uploadedProtocol;
  String uploadedExtruders;
  String uploadedUuid;
  String uploadedEvidence;
  String installedFirmware;
  String installedMachine;
  String installedBoard;
  String installedMcu;
  String installedSourceUrl;
  String installedProtocol;
  String installedExtruders;
  String installedUuid;
  bool comparisonComplete{false};
  bool identical{false};
  String message{"Waiting for firmware"};
};

extern JobStatus gStatus;

const char *jobStateName(JobState state);
void setStatus(JobState state, uint8_t progress, const String &message);
