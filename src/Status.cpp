#include "Status.hpp"

#include "BoardConfig.hpp"

JobStatus gStatus;

const char *jobStateName(JobState state) {
  switch (state) {
  case JobState::Idle: return "idle";
  case JobState::Uploading: return "uploading";
  case JobState::Probing: return "probing";
  case JobState::Ready: return "ready";
  case JobState::Current: return "current";
  case JobState::Connecting: return "connecting";
  case JobState::BackingUp: return "backing_up";
  case JobState::Erasing: return "erasing";
  case JobState::Programming: return "programming";
  case JobState::Verifying: return "verifying";
  case JobState::Restoring: return "restoring";
  case JobState::Success: return "success";
  case JobState::Error: return "error";
  }
  return "unknown";
}

static void writeLed(int pin, bool on) {
  if (pin >= 0) digitalWrite(pin, on ? HIGH : LOW);
}

void setStatus(JobState state, uint8_t progress, const String &message) {
  gStatus.state = state;
  gStatus.progress = progress;
  gStatus.message = message;

  const bool active = state == JobState::Probing || state == JobState::Connecting || state == JobState::BackingUp ||
                      state == JobState::Erasing || state == JobState::Programming ||
                      state == JobState::Verifying || state == JobState::Restoring;
  writeLed(board::kFlashingLed, active);
  writeLed(board::kSuccessLed, state == JobState::Success);
  writeLed(board::kErrorLed, state == JobState::Error);
}
