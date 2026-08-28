#pragma once

#include <Arduino.h>

class SwdTransport {
public:
  void begin();
  bool connect();
  void disconnect();
  void assertReset(bool asserted);
  float targetVoltage() const;

  bool read32(uint32_t address, uint32_t &value);
  bool write32(uint32_t address, uint32_t value);
  bool readBlock(uint32_t address, uint8_t *data, size_t size);
  bool writeBlock(uint32_t address, const uint8_t *data, size_t size);

  uint32_t debugPortId() const { return dpidr_; }
  const String &lastError() const { return error_; }

private:
  enum : uint8_t {
    kAp = 1U << 0,
    kRead = 1U << 1,
    kA2 = 1U << 2,
    kA3 = 1U << 3,
  };
  enum : uint8_t { kAckOk = 1, kAckWait = 2, kAckFault = 4 };
  enum : uint8_t {
    kDpIdcode = 0x00,
    kDpAbort = 0x00,
    kDpCtrlStat = 0x04,
    kDpSelect = 0x08,
    kDpRdbuff = 0x0c,
    kApCsw = 0x00 | kAp,
    kApTar = 0x04 | kAp,
    kApDrw = 0x0c | kAp,
  };

  void swdioOutput();
  void swdioInput();
  void clockCycles(unsigned count);
  void writeBits(uint32_t value, unsigned count);
  uint32_t readBits(unsigned count);
  static uint8_t parity(uint32_t value);
  uint8_t operation(uint8_t request, uint32_t *data);
  bool transfer(uint8_t request, uint32_t *data);
  bool readRegister(uint8_t reg, uint32_t &value);
  bool writeRegister(uint8_t reg, uint32_t value);
  void lineReset();
  bool prepareAccessPort();
  void setError(const String &message);

  uint32_t dpidr_{0};
  String error_;
};

