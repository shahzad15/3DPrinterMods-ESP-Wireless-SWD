/* STM32G0 flash-controller sequence based on RM0444 and ataradov/edbg. */
#include "Stm32G0Programmer.hpp"

#include <LittleFS.h>

#include "BoardConfig.hpp"

namespace {
constexpr uint32_t kDhcsr = 0xe000edf0;
constexpr uint32_t kDhcsrKey = 0xa05f0000;
constexpr uint32_t kDhcsrDebugEnable = 1U << 0;
constexpr uint32_t kDhcsrHalt = 1U << 1;
constexpr uint32_t kDemcr = 0xe000edfc;
constexpr uint32_t kDemcrVectorCatchReset = 1U << 0;
constexpr uint32_t kAircr = 0xe000ed0c;
constexpr uint32_t kAircrKey = 0x05fa0000;
constexpr uint32_t kAircrSystemReset = 1U << 2;
constexpr uint32_t kDebugIdCode = 0x40015800;
constexpr uint32_t kFlashSize = 0x1fff75e0;
constexpr uint32_t kUniqueId = 0x1fff7590;

constexpr uint32_t kFlashKeyr = 0x40022008;
constexpr uint32_t kFlashSr = 0x40022010;
constexpr uint32_t kFlashCr = 0x40022014;
constexpr uint32_t kFlashKey1 = 0x45670123;
constexpr uint32_t kFlashKey2 = 0xcdef89ab;
constexpr uint32_t kFlashSrEop = 1U << 0;
constexpr uint32_t kFlashSrErrors = (1U << 1) | (1U << 3) | (1U << 4) |
                                     (1U << 5) | (1U << 6) | (1U << 7) |
                                     (1U << 8) | (1U << 9) | (1U << 14) |
                                     (1U << 15);
constexpr uint32_t kFlashSrBusy = 1U << 16;
constexpr uint32_t kFlashCrProgram = 1U << 0;
constexpr uint32_t kFlashCrPageErase = 1U << 1;
constexpr uint32_t kFlashCrPageNumber(uint32_t page) { return (page & 0x3ffU) << 3U; }
constexpr uint32_t kFlashCrStart = 1U << 16;
constexpr uint32_t kFlashCrLock = 1UL << 31;
constexpr size_t kPageSize = 2048;
constexpr size_t kTransferSize = 256;

size_t roundUpPage(size_t value) {
  return (value + kPageSize - 1U) & ~(kPageSize - 1U);
}

uint32_t readLe32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8U) |
         (static_cast<uint32_t>(p[2]) << 16U) |
         (static_cast<uint32_t>(p[3]) << 24U);
}
} // namespace

bool Stm32G0Programmer::check(bool result, const String &context) {
  if (result) return true;
  error_ = context + ": " + swd_.lastError();
  return false;
}

bool Stm32G0Programmer::attach(TargetInfo &info) {
  error_ = "";
  const float vref = swd_.targetVoltage();
  if (vref < 2.7f || vref > 3.6f) {
    error_ = "Target VTREF is not a valid 3.3 V rail (measured " + String(vref, 2) + " V)";
    return false;
  }

  swd_.assertReset(true);
  delay(5);
  if (!check(swd_.connect(), "SWD connection failed")) return false;
  if (!check(swd_.write32(kDhcsr, kDhcsrKey | kDhcsrDebugEnable | kDhcsrHalt),
             "Could not halt target")) return false;
  if (!check(swd_.write32(kDemcr, kDemcrVectorCatchReset), "Could not set reset catch")) return false;
  swd_.assertReset(false);
  delay(10);
  if (!check(swd_.write32(kDhcsr, kDhcsrKey | kDhcsrDebugEnable | kDhcsrHalt),
             "Could not halt target after reset")) return false;

  uint32_t id = 0;
  uint32_t flashSizeWord = 0;
  if (!check(swd_.read32(kDebugIdCode, id), "Could not read STM32 device ID")) return false;
  if (!check(swd_.read32(kFlashSize, flashSizeWord), "Could not read flash size")) return false;
  info.debugPortId = swd_.debugPortId();
  info.deviceIdCode = id;
  info.deviceId = id & 0x0fffU;
  info.revisionId = static_cast<uint16_t>(id >> 16U);
  info.flashBytes = (flashSizeWord & 0xffffU) * 1024U;
  if (info.deviceId != 0x467U) {
    error_ = "Refusing unknown MCU; expected STM32G0B/G0C device ID 0x467, got 0x" +
             String(info.deviceId, HEX);
    return false;
  }
  if (info.flashBytes != 256U * 1024U && info.flashBytes != 512U * 1024U) {
    error_ = "Unexpected STM32 flash size: " + String(info.flashBytes);
    return false;
  }
  for (size_t i = 0; i < 3; ++i) {
    if (!check(swd_.read32(kUniqueId + i * 4U, info.uniqueId[i]),
               "Could not read STM32 unique ID")) return false;
  }
  info.model = "STM32G0B1/G0C1 family (DEV_ID 0x467)";
  char uid[25];
  snprintf(uid, sizeof(uid), "%08lX%08lX%08lX",
           static_cast<unsigned long>(info.uniqueId[2]),
           static_cast<unsigned long>(info.uniqueId[1]),
           static_cast<unsigned long>(info.uniqueId[0]));
  info.uniqueIdText = uid;
  return true;
}

bool Stm32G0Programmer::inspectApplication(fs::FS &fs, const char *uploadPath,
                                           uint32_t offset, size_t size,
                                           ApplicationInspection &inspection,
                                           ProgressCallback progress) {
  File expected = fs.open(uploadPath, "r");
  if (!expected) { error_ = "Uploaded firmware is missing during preflight"; return false; }

  FirmwareMetadataScanner scanner;
  uint8_t expectedBytes[kTransferSize];
  uint8_t actualBytes[kTransferSize];
  inspection.identicalToUpload = true;
  uint32_t crc = 0xffffffffU;
  const size_t total = roundUpPage(size);
  size_t done = 0;
  while (done < total) {
    memset(expectedBytes, 0xff, sizeof(expectedBytes));
    const size_t fileRemaining = size > done ? size - done : 0;
    const size_t fromFile = fileRemaining < kTransferSize ? fileRemaining : kTransferSize;
    if (fromFile && static_cast<size_t>(expected.read(expectedBytes, fromFile)) != fromFile) {
      expected.close();
      error_ = "Short upload read during preflight";
      return false;
    }
    if (!check(swd_.readBlock(board::kFlashBase + offset + done, actualBytes, kTransferSize),
               "Installed firmware read failed")) {
      expected.close();
      return false;
    }
    if (inspection.identicalToUpload && memcmp(expectedBytes, actualBytes, kTransferSize) != 0)
      inspection.identicalToUpload = false;
    if (done == 0) {
      inspection.initialSp = readLe32(actualBytes);
      inspection.resetVector = readLe32(actualBytes + 4);
    }
    if (fromFile) {
      scanner.feed(actualBytes, fromFile);
      crc = updateCrc32(crc, actualBytes, fromFile);
    }
    done += kTransferSize;
    progress(static_cast<uint8_t>(done * 100U / total), "Reading and comparing installed firmware");
    delay(0);
  }
  expected.close();
  scanner.finish();
  inspection.crc32 = ~crc;
  inspection.metadata = scanner.metadata();
  return true;
}

void Stm32G0Programmer::detachAndRun() {
  (void)swd_.write32(kDemcr, 0);
  (void)swd_.write32(kAircr, kAircrKey | kAircrSystemReset);
  delay(10);
  swd_.disconnect();
  swd_.assertReset(false);
}

bool Stm32G0Programmer::unlockFlash() {
  uint32_t cr = 0;
  if (!check(swd_.read32(kFlashCr, cr), "Could not read FLASH_CR")) return false;
  if (cr & kFlashCrLock) {
    if (!check(swd_.write32(kFlashKeyr, kFlashKey1), "FLASH key 1 failed") ||
        !check(swd_.write32(kFlashKeyr, kFlashKey2), "FLASH key 2 failed")) return false;
    if (!check(swd_.read32(kFlashCr, cr), "Could not confirm flash unlock")) return false;
    if (cr & kFlashCrLock) {
      error_ = "Flash remains locked (readout/write protection may be enabled)";
      return false;
    }
  }
  return check(swd_.write32(kFlashSr, kFlashSrErrors | kFlashSrEop),
               "Could not clear FLASH status");
}

bool Stm32G0Programmer::waitFlash(uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  uint32_t sr = 0;
  do {
    if (!check(swd_.read32(kFlashSr, sr), "Could not poll FLASH_SR")) return false;
    if ((sr & kFlashSrBusy) == 0) break;
    delay(1);
  } while (static_cast<int32_t>(deadline - millis()) > 0);
  if (sr & kFlashSrBusy) {
    error_ = "STM32 flash operation timed out";
    return false;
  }
  if (sr & kFlashSrErrors) {
    error_ = "STM32 FLASH_SR error flags: 0x" + String(sr, HEX);
    return false;
  }
  return true;
}

bool Stm32G0Programmer::erasePage(uint32_t page) {
  const uint32_t command = kFlashCrPageErase | kFlashCrPageNumber(page);
  return check(swd_.write32(kFlashCr, command), "Could not configure page erase") &&
         check(swd_.write32(kFlashCr, command | kFlashCrStart), "Could not start page erase") &&
         waitFlash(3000) && check(swd_.write32(kFlashCr, 0), "Could not finish page erase");
}

bool Stm32G0Programmer::backup(fs::FS &fs, const char *path, uint32_t offset,
                               size_t size, ProgressCallback progress) {
  File out = fs.open(path, "w");
  if (!out) { error_ = "Could not create recovery backup"; return false; }
  uint8_t buffer[kTransferSize];
  const size_t total = roundUpPage(size);
  for (size_t done = 0; done < total; done += sizeof(buffer)) {
    if (!check(swd_.readBlock(board::kFlashBase + offset + done, buffer, sizeof(buffer)),
               "Backup read failed")) { out.close(); return false; }
    if (out.write(buffer, sizeof(buffer)) != sizeof(buffer)) {
      error_ = "LittleFS write failed during backup";
      out.close();
      return false;
    }
    progress(static_cast<uint8_t>((done + sizeof(buffer)) * 100U / total), "Saving recovery backup");
    delay(0);
  }
  out.close();
  return true;
}

bool Stm32G0Programmer::programPage(File &file, uint32_t address, size_t bytesFromFile) {
  uint8_t buffer[kTransferSize];
  if (!check(swd_.write32(kFlashCr, kFlashCrProgram), "Could not enable programming")) return false;
  for (size_t pageOffset = 0; pageOffset < kPageSize; pageOffset += sizeof(buffer)) {
    memset(buffer, 0xff, sizeof(buffer));
    const size_t wanted = bytesFromFile < sizeof(buffer) ? bytesFromFile : sizeof(buffer);
    if (wanted && static_cast<size_t>(file.read(buffer, wanted)) != wanted) {
      error_ = "Short read from firmware image";
      return false;
    }
    bytesFromFile -= wanted;
    // STM32G0 standard programming is exactly one aligned 64-bit doubleword
    // at a time. Waiting after each pair is intentionally slower than fast-row
    // mode, but avoids its strict timing requirement when Wi-Fi preempts GPIO.
    for (size_t offset = 0; offset < sizeof(buffer); offset += 8) {
      if (!check(swd_.writeBlock(address + pageOffset + offset, buffer + offset, 8),
                 "Flash doubleword write failed")) return false;
      if (!waitFlash(3000)) return false;
      if (!check(swd_.write32(kFlashSr, kFlashSrEop), "Could not clear FLASH EOP")) return false;
    }
    delay(0);
  }
  return check(swd_.write32(kFlashCr, 0), "Could not disable programming");
}

bool Stm32G0Programmer::program(fs::FS &fs, const char *path, uint32_t offset,
                                size_t size, ProgressCallback progress) {
  File file = fs.open(path, "r");
  if (!file) { error_ = "Firmware file disappeared"; return false; }
  if (!unlockFlash() ||
      !check(swd_.write32(kFlashCr, 0), "Could not reset FLASH control") ||
      !check(swd_.write32(kFlashSr, kFlashSrErrors | kFlashSrEop),
             "Could not clear FLASH status before programming")) {
    file.close();
    return false;
  }
  const size_t total = roundUpPage(size);
  for (size_t done = 0; done < total; done += kPageSize) {
    const uint32_t page = (offset + done) / kPageSize;
    progress(static_cast<uint8_t>(done * 100U / total), "Erasing next application page");
    if (!erasePage(page)) { file.close(); return false; }
    const size_t remaining = size > done ? size - done : 0;
    const size_t bytesThisPage = remaining < kPageSize ? remaining : kPageSize;
    progress(static_cast<uint8_t>((done + kPageSize / 2U) * 100U / total),
             "Programming firmware page");
    if (!programPage(file, board::kFlashBase + offset + done, bytesThisPage)) {
      file.close();
      return false;
    }
  }
  file.close();
  progress(100, "Programming complete");
  return true;
}

bool Stm32G0Programmer::verify(fs::FS &fs, const char *path, uint32_t offset,
                               size_t size, ProgressCallback progress) {
  File file = fs.open(path, "r");
  if (!file) { error_ = "Verification source is missing"; return false; }
  uint8_t expected[kTransferSize];
  uint8_t actual[kTransferSize];
  size_t done = 0;
  while (done < size) {
    const size_t chunk = (size - done) < sizeof(expected) ? (size - done) : sizeof(expected);
    memset(expected, 0xff, sizeof(expected));
    if (static_cast<size_t>(file.read(expected, chunk)) != chunk) {
      error_ = "Short verification file read";
      file.close();
      return false;
    }
    if (!check(swd_.readBlock(board::kFlashBase + offset + done, actual, sizeof(actual)),
               "Verification SWD read failed")) return false;
    if (memcmp(expected, actual, chunk) != 0) {
      size_t index = 0;
      while (index < chunk && expected[index] == actual[index]) ++index;
      error_ = "Verify mismatch at 0x" + String(board::kFlashBase + offset + done + index, HEX);
      file.close();
      return false;
    }
    done += chunk;
    progress(static_cast<uint8_t>(done * 100U / size), "Verifying flash");
    delay(0);
  }
  file.close();
  return true;
}
