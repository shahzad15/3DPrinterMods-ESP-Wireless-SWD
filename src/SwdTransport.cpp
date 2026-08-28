/*
 * SWD transaction structure adapted from Alex Taradov's embedded-swd project.
 * Copyright (c) 2014-2018 Alex Taradov. BSD-3-Clause; see LICENSES/BSD-3-Clause.txt.
 */
#include "SwdTransport.hpp"

#include "HardwareAbstraction.hpp"

namespace {
constexpr unsigned kRetryCount = 128;

uint32_t load32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8U) |
         (static_cast<uint32_t>(p[2]) << 16U) |
         (static_cast<uint32_t>(p[3]) << 24U);
}

void store32(uint8_t *p, uint32_t value) {
  p[0] = value;
  p[1] = value >> 8U;
  p[2] = value >> 16U;
  p[3] = value >> 24U;
}
} // namespace

void SwdTransport::begin() {
  hal::initializeSwd();
}

void SwdTransport::swdioOutput() { hal::setSwdioOutput(true); }
void SwdTransport::swdioInput() { hal::setSwdioOutput(false); }

void SwdTransport::clockCycles(unsigned count) {
  while (count--) {
    hal::writeSwclk(false);
    hal::swdHalfPeriodDelay();
    hal::writeSwclk(true);
    hal::swdHalfPeriodDelay();
  }
}

void SwdTransport::writeBits(uint32_t value, unsigned count) {
  for (unsigned i = 0; i < count; ++i) {
    hal::writeSwdio(value & 1U);
    hal::writeSwclk(false);
    hal::swdHalfPeriodDelay();
    hal::writeSwclk(true);
    hal::swdHalfPeriodDelay();
    value >>= 1U;
  }
}

uint32_t SwdTransport::readBits(unsigned count) {
  uint32_t value = 0;
  for (unsigned i = 0; i < count; ++i) {
    hal::writeSwclk(false);
    hal::swdHalfPeriodDelay();
    if (hal::readSwdio()) value |= 1UL << i;
    hal::writeSwclk(true);
    hal::swdHalfPeriodDelay();
  }
  return value;
}

uint8_t SwdTransport::parity(uint32_t value) {
  value ^= value >> 16U;
  value ^= value >> 8U;
  value ^= value >> 4U;
  return (0x6996U >> (value & 0x0fU)) & 1U;
}

uint8_t SwdTransport::operation(uint8_t request, uint32_t *data) {
  const uint8_t requestBits = request & (kAp | kRead | kA2 | kA3);
  writeBits(0x81U | (parity(requestBits) << 5U) | (requestBits << 1U), 8);
  swdioInput();
  clockCycles(1); // Host-to-target turnaround.
  uint8_t ack = readBits(3);

  if (ack == kAckOk) {
    if (requestBits & kRead) {
      const uint32_t value = readBits(32);
      if (parity(value) != readBits(1)) ack = 8;
      if (data) *data = value;
      clockCycles(1);
      swdioOutput();
    } else {
      clockCycles(1);
      swdioOutput();
      writeBits(*data, 32);
      writeBits(parity(*data), 1);
    }
  } else {
    // WAIT/FAULT has no data phase. An invalid ACK may mean the target never
    // took the line, so consume a complete turnaround/data/parity window to
    // return to a known request boundary.
    clockCycles((ack == kAckWait || ack == kAckFault) ? 1 : 34);
    swdioOutput();
  }
  hal::writeSwdio(true);
  return ack;
}

bool SwdTransport::transfer(uint8_t request, uint32_t *data) {
  uint8_t ack = 0;
  for (unsigned attempt = 0; attempt < kRetryCount; ++attempt) {
    ack = operation(request, data);
    if (ack != kAckWait) break;
  }
  if (ack != kAckOk) {
    setError(ack == kAckFault ? "SWD FAULT response" :
             ack == kAckWait ? "SWD WAIT timeout" : "Invalid SWD response/parity");
    return false;
  }

  if (request & kRead) {
    if (request & kAp) {
      uint32_t posted = 0;
      if (operation(kDpRdbuff | kRead, &posted) != kAckOk) {
        setError("Failed to collect posted AP read");
        return false;
      }
      if (data) *data = posted;
    }
  } else {
    uint32_t ignored = 0;
    if (operation(kDpRdbuff | kRead, &ignored) != kAckOk) {
      setError("SWD write completion failed");
      return false;
    }
  }
  return true;
}

bool SwdTransport::readRegister(uint8_t reg, uint32_t &value) {
  return transfer(reg | kRead, &value);
}

bool SwdTransport::writeRegister(uint8_t reg, uint32_t value) {
  return transfer(reg, &value);
}

void SwdTransport::lineReset() {
  swdioOutput();
  hal::writeSwdio(true);
  clockCycles(64);
  // JTAG-to-SWD select sequence 0xE79E, least-significant bit first.
  writeBits(0xe79e, 16);
  hal::writeSwdio(true);
  clockCycles(64);
  hal::writeSwdio(false);
  clockCycles(8);
}

bool SwdTransport::prepareAccessPort() {
  if (!writeRegister(kDpAbort, 0x1e)) return false;
  if (!writeRegister(kDpSelect, 0)) return false;
  if (!writeRegister(kDpCtrlStat, 0x50000000)) return false;

  const uint32_t deadline = millis() + 250;
  uint32_t status = 0;
  do {
    if (!readRegister(kDpCtrlStat, status)) return false;
    if ((status & 0xa0000000U) == 0xa0000000U) break;
  } while (static_cast<int32_t>(deadline - millis()) > 0);
  if ((status & 0xa0000000U) != 0xa0000000U) {
    setError("Debug/access-port power-up timeout");
    return false;
  }
  if (!writeRegister(kDpSelect, 0)) return false;
  return writeRegister(kApCsw, 0x23000052);
}

bool SwdTransport::connect() {
  error_ = "";
  hal::connectSwdPins();
  lineReset();
  if (!readRegister(kDpIdcode, dpidr_)) return false;
  if (dpidr_ == 0 || dpidr_ == 0xffffffffU) {
    setError("No ARM SWD debug port detected");
    return false;
  }
  return prepareAccessPort();
}

void SwdTransport::disconnect() {
  hal::disconnectSwdPins();
}

void SwdTransport::assertReset(bool asserted) {
  hal::setTargetReset(asserted);
}

float SwdTransport::targetVoltage() const {
  return hal::readTargetVoltage();
}

bool SwdTransport::read32(uint32_t address, uint32_t &value) {
  if (!writeRegister(kApTar, address)) return false;
  return readRegister(kApDrw, value);
}

bool SwdTransport::write32(uint32_t address, uint32_t value) {
  return writeRegister(kApTar, address) && writeRegister(kApDrw, value);
}

bool SwdTransport::readBlock(uint32_t address, uint8_t *data, size_t size) {
  if ((address & 3U) || (size & 3U)) {
    setError("SWD block read is not word aligned");
    return false;
  }
  while (size) {
    const size_t boundary = 0x400U - (address & 0x3ffU);
    const size_t chunk = size < boundary ? size : boundary;
    if (!writeRegister(kApTar, address)) return false;
    for (size_t offset = 0; offset < chunk; offset += 4) {
      uint32_t value = 0;
      if (!readRegister(kApDrw, value)) return false;
      store32(data + offset, value);
    }
    address += chunk;
    data += chunk;
    size -= chunk;
  }
  return true;
}

bool SwdTransport::writeBlock(uint32_t address, const uint8_t *data, size_t size) {
  if ((address & 3U) || (size & 3U)) {
    setError("SWD block write is not word aligned");
    return false;
  }
  while (size) {
    const size_t boundary = 0x400U - (address & 0x3ffU);
    const size_t chunk = size < boundary ? size : boundary;
    if (!writeRegister(kApTar, address)) return false;
    for (size_t offset = 0; offset < chunk; offset += 4) {
      if (!writeRegister(kApDrw, load32(data + offset))) return false;
    }
    address += chunk;
    data += chunk;
    size -= chunk;
  }
  return true;
}

void SwdTransport::setError(const String &message) { error_ = message; }
