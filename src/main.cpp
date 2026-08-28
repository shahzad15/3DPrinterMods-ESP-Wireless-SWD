#include <Arduino.h>
#include <LittleFS.h>

#if defined(ESP32)
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
using HttpServer = WebServer;
#elif defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
}
using HttpServer = ESP8266WebServer;
#endif

#include "BoardConfig.hpp"
#include "FirmwareImage.hpp"
#include "Status.hpp"
#include "Stm32G0Programmer.hpp"
#include "SwdTransport.hpp"
#include "WebUi.hpp"

#if __has_include("WifiSecrets.hpp")
#include "WifiSecrets.hpp"
#define HAS_LOCAL_WIFI_CONFIG 1
#else
#define HAS_LOCAL_WIFI_CONFIG 0
#endif

namespace {
constexpr const char *kFirmwarePath = "/firmware.bin";
constexpr const char *kBackupPath = "/backup.bin";
// Deliberately fixed for this local-only installation, per the owner's request.
// This protects both HTTP Basic authentication and the fallback Wi-Fi AP.
constexpr const char *kAdminPassword = "myadminpass";

HttpServer server(80);
SwdTransport swd;
Stm32G0Programmer programmer(swd);
String apPassword{kAdminPassword};
String csrfToken;
File uploadFile;
bool uploadFailed = false;
bool jobRunning = false;
enum class PendingJob { None, Probe, Flash, Restore };
PendingJob pendingJob = PendingJob::None;

uint32_t secureRandom32() {
#if defined(ESP32)
  return esp_random();
#else
  return os_random();
#endif
}

String randomText(size_t length) {
  static constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  String result;
  result.reserve(length);
  while (result.length() < length) result += alphabet[secureRandom32() % (sizeof(alphabet) - 1U)];
  return result;
}

bool authorized() {
  if (!server.authenticate("admin", apPassword.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

bool validCsrf() {
  return server.hasHeader("X-CSRF-Token") && server.header("X-CSRF-Token") == csrfToken;
}

String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (static_cast<uint8_t>(c) >= 0x20) out += c;
  }
  return out;
}

void sendStatus() {
  if (!authorized()) return;
  String body = "{\"state\":\"" + String(jobStateName(gStatus.state)) +
                "\",\"progress\":" + String(gStatus.progress) +
                ",\"message\":\"" + jsonEscape(gStatus.message) +
                "\",\"image_size\":" + String(gStatus.imageSize) +
                ",\"image_load_address\":" + String(gStatus.imageLoadAddress) +
                ",\"uploaded_initial_sp\":" + String(gStatus.uploadedInitialSp) +
                ",\"uploaded_reset_vector\":" + String(gStatus.uploadedResetVector) +
                ",\"uploaded_crc32\":" + String(gStatus.uploadedCrc32) +
                ",\"installed_initial_sp\":" + String(gStatus.installedInitialSp) +
                ",\"installed_reset_vector\":" + String(gStatus.installedResetVector) +
                ",\"installed_crc32\":" + String(gStatus.installedCrc32) +
                ",\"target_debug_port_id\":" + String(gStatus.targetDebugPortId) +
                ",\"target_id\":" + String(gStatus.targetId) +
                ",\"target_id_code\":" + String(gStatus.targetIdCode) +
                ",\"target_revision\":" + String(gStatus.targetRevision) +
                ",\"flash_bytes\":" + String(gStatus.flashBytes) +
                ",\"target_model\":\"" + jsonEscape(gStatus.targetModel) +
                "\",\"target_uid\":\"" + jsonEscape(gStatus.targetUniqueId) +
                "\",\"uploaded_firmware\":\"" + jsonEscape(gStatus.uploadedFirmware) +
                "\",\"uploaded_machine\":\"" + jsonEscape(gStatus.uploadedMachine) +
                "\",\"uploaded_board\":\"" + jsonEscape(gStatus.uploadedBoard) +
                "\",\"uploaded_mcu\":\"" + jsonEscape(gStatus.uploadedMcu) +
                "\",\"uploaded_source_url\":\"" + jsonEscape(gStatus.uploadedSourceUrl) +
                "\",\"uploaded_protocol\":\"" + jsonEscape(gStatus.uploadedProtocol) +
                "\",\"uploaded_extruders\":\"" + jsonEscape(gStatus.uploadedExtruders) +
                "\",\"uploaded_uuid\":\"" + jsonEscape(gStatus.uploadedUuid) +
                "\",\"uploaded_evidence\":\"" + jsonEscape(gStatus.uploadedEvidence) +
                "\",\"installed_firmware\":\"" + jsonEscape(gStatus.installedFirmware) +
                "\",\"installed_machine\":\"" + jsonEscape(gStatus.installedMachine) +
                "\",\"installed_board\":\"" + jsonEscape(gStatus.installedBoard) +
                "\",\"installed_mcu\":\"" + jsonEscape(gStatus.installedMcu) +
                "\",\"installed_source_url\":\"" + jsonEscape(gStatus.installedSourceUrl) +
                "\",\"installed_protocol\":\"" + jsonEscape(gStatus.installedProtocol) +
                "\",\"installed_extruders\":\"" + jsonEscape(gStatus.installedExtruders) +
                "\",\"installed_uuid\":\"" + jsonEscape(gStatus.installedUuid) +
                "\",\"comparison_complete\":" + String(gStatus.comparisonComplete ? "true" : "false") +
                ",\"identical\":" + String(gStatus.identical ? "true" : "false") + "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}

void backupProgress(uint8_t percent, const String &message) {
  setStatus(JobState::BackingUp, percent, message);
}

void programProgress(uint8_t percent, const String &message) {
  const JobState state = message.startsWith("Eras") ? JobState::Erasing : JobState::Programming;
  setStatus(state, percent, message);
}

void verifyProgress(uint8_t percent, const String &message) {
  setStatus(JobState::Verifying, percent, message);
}

void probeProgress(uint8_t percent, const String &message) {
  setStatus(JobState::Probing, percent, message);
}

void storeTarget(const TargetInfo &target) {
  gStatus.targetDebugPortId = target.debugPortId;
  gStatus.targetId = target.deviceId;
  gStatus.targetIdCode = target.deviceIdCode;
  gStatus.targetRevision = target.revisionId;
  gStatus.flashBytes = target.flashBytes;
  gStatus.targetModel = target.model;
  gStatus.targetUniqueId = target.uniqueIdText;
}

bool restoreBackup(TargetInfo &target, String &failure) {
  File backup = LittleFS.open(kBackupPath, "r");
  if (!backup) { failure = "No recovery backup exists"; return false; }
  const size_t backupSize = backup.size();
  backup.close();
  if (backupSize == 0 || board::kMarlinOffset + backupSize > target.flashBytes) {
    failure = "Recovery backup size does not match this target";
    return false;
  }
  setStatus(JobState::Restoring, 0, "Restoring the previous application");
  if (!programmer.program(LittleFS, kBackupPath, board::kMarlinOffset, backupSize,
                          [](uint8_t p, const String &m) { setStatus(JobState::Restoring, p / 2, m); })) {
    failure = programmer.lastError();
    return false;
  }
  if (!programmer.verify(LittleFS, kBackupPath, board::kMarlinOffset, backupSize,
                         [](uint8_t p, const String &m) { setStatus(JobState::Restoring, 50 + p / 2, m); })) {
    failure = programmer.lastError();
    return false;
  }
  return true;
}

void executeJob(PendingJob job) {
  TargetInfo target;
  String failure;
  const String expectedTargetUid = gStatus.targetUniqueId;
  setStatus(job == PendingJob::Probe ? JobState::Probing : JobState::Connecting,
            0, job == PendingJob::Probe ? "Read-only SWD preflight" : "Connecting under reset");
  if (!programmer.attach(target)) {
    failure = programmer.lastError();
    goto failed;
  }
  if (job != PendingJob::Probe && expectedTargetUid.length() &&
      target.uniqueIdText != expectedTargetUid) {
    failure = "The connected controller changed after preflight; upload and probe again";
    goto attached_failed;
  }
  storeTarget(target);

  if (job == PendingJob::Probe) {
    if (board::kMarlinOffset + gStatus.imageSize > target.flashBytes) {
      failure = "Uploaded firmware is too large for the detected controller";
      goto attached_failed;
    }
    if (gStatus.uploadedBoard.length() &&
        gStatus.uploadedBoard != "BTT SKR Mini E3 V3.0") {
      failure = "Uploaded binary identifies a different board: " + gStatus.uploadedBoard;
      goto attached_failed;
    }
    if (gStatus.uploadedMcu.length() &&
        gStatus.uploadedMcu != "STM32G0B1" && gStatus.uploadedMcu != "STM32G0C1") {
      failure = "Uploaded binary identifies a different MCU: " + gStatus.uploadedMcu;
      goto attached_failed;
    }
    ApplicationInspection inspection;
    if (!programmer.inspectApplication(LittleFS, kFirmwarePath, board::kMarlinOffset,
                                       gStatus.imageSize, inspection, probeProgress)) {
      failure = programmer.lastError();
      goto attached_failed;
    }
    gStatus.installedFirmware = inspection.metadata.firmwareName;
    gStatus.installedMachine = inspection.metadata.machineName;
    gStatus.installedBoard = inspection.metadata.boardName;
    gStatus.installedMcu = inspection.metadata.mcuName;
    gStatus.installedSourceUrl = inspection.metadata.sourceCodeUrl;
    gStatus.installedProtocol = inspection.metadata.protocolVersion;
    gStatus.installedExtruders = inspection.metadata.extruderCount;
    gStatus.installedUuid = inspection.metadata.uuid;
    gStatus.installedInitialSp = inspection.initialSp;
    gStatus.installedResetVector = inspection.resetVector;
    gStatus.installedCrc32 = inspection.crc32;
    gStatus.uploadedEvidence = "Live target confirmed as " + target.model +
        "; image fits detected flash; vector table/link address valid";
    if (gStatus.uploadedBoard.length()) gStatus.uploadedEvidence += "; embedded board matches";
    else gStatus.uploadedEvidence += "; board name not embedded";
    if (gStatus.uploadedMcu.length()) gStatus.uploadedEvidence += "; embedded MCU matches";
    else gStatus.uploadedEvidence += "; exact MCU name not embedded";
    gStatus.comparisonComplete = true;
    gStatus.identical = inspection.identicalToUpload;
    programmer.detachAndRun();
    if (inspection.identicalToUpload) {
      setStatus(JobState::Current, 100,
                "The uploaded image is already installed; flashing is disabled");
    } else {
      setStatus(JobState::Ready, 100,
                "Preflight passed: controller matches and uploaded image differs");
    }
    jobRunning = false;
    return;
  }

  if (job == PendingJob::Restore) {
    if (!restoreBackup(target, failure)) goto attached_failed;
    programmer.detachAndRun();
    setStatus(JobState::Success, 100, "Previous firmware restored and verified");
    jobRunning = false;
    return;
  }

  if (board::kMarlinOffset + gStatus.imageSize > target.flashBytes) {
    failure = "Firmware is too large for the detected MCU";
    goto attached_failed;
  }
  if (!programmer.backup(LittleFS, kBackupPath, board::kMarlinOffset,
                         gStatus.imageSize, backupProgress)) {
    failure = programmer.lastError();
    goto attached_failed;
  }
  if (!programmer.program(LittleFS, kFirmwarePath, board::kMarlinOffset,
                          gStatus.imageSize, programProgress) ||
      !programmer.verify(LittleFS, kFirmwarePath, board::kMarlinOffset,
                         gStatus.imageSize, verifyProgress)) {
    const String updateFailure = programmer.lastError();
    if (restoreBackup(target, failure)) {
      programmer.detachAndRun();
      setStatus(JobState::Error, 100, "Update failed; original firmware was restored. Cause: " + updateFailure);
      jobRunning = false;
      return;
    }
    failure = updateFailure + "; automatic restore also failed: " + failure;
    goto attached_failed;
  }
  programmer.detachAndRun();
  setStatus(JobState::Success, 100, "Firmware programmed, byte-verified, and target reset");
  jobRunning = false;
  return;

attached_failed:
failed:
  // Safe even after a partial attach: best-effort clear vector catch, request
  // reset, release the open-drain reset output, and return SWD pins to Hi-Z.
  programmer.detachAndRun();
  setStatus(JobState::Error, gStatus.progress, failure);
  jobRunning = false;
}

#if defined(ESP32)
void jobTask(void *parameter) {
  const PendingJob job = static_cast<PendingJob>(reinterpret_cast<uintptr_t>(parameter));
  executeJob(job);
  vTaskDelete(nullptr);
}
#endif

void scheduleJob(PendingJob job) {
  if (jobRunning) { server.send(409, "text/plain", "A flash job is already running"); return; }
  jobRunning = true;
  server.send(202, "text/plain", "Accepted");
#if defined(ESP32)
  xTaskCreatePinnedToCore(jobTask, "swd-flash", 8192,
                         reinterpret_cast<void *>(static_cast<uintptr_t>(job)), 1, nullptr,
                         ARDUINO_RUNNING_CORE);
#else
  pendingJob = job; // Run after the HTTP response has been flushed.
#endif
}

void handleUploadData() {
  if (!server.authenticate("admin", apPassword.c_str()) || !validCsrf() || jobRunning) return;
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadFailed = false;
    LittleFS.remove(kFirmwarePath);
    uploadFile = LittleFS.open(kFirmwarePath, "w");
    if (!uploadFile) uploadFailed = true;
    setStatus(JobState::Uploading, 0, "Receiving firmware");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadFailed && upload.totalSize <= board::kMaximumImageSize &&
        uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) uploadFailed = true;
    if (upload.totalSize > board::kMaximumImageSize) uploadFailed = true;
  } else if (upload.status == UPLOAD_FILE_END || upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    if (upload.status == UPLOAD_FILE_ABORTED) uploadFailed = true;
  }
}

void finishUpload() {
  if (!authorized()) return;
  if (!validCsrf()) { server.send(403, "text/plain", "Invalid request token"); return; }
  if (jobRunning) { server.send(409, "text/plain", "A preflight or flash job is already running"); return; }
  if (uploadFailed) {
    LittleFS.remove(kFirmwarePath);
    setStatus(JobState::Error, 0, "Upload failed or image exceeded the size limit");
    server.send(400, "text/plain", gStatus.message);
    return;
  }
  const ImageInfo image = inspectMarlinImage(LittleFS, kFirmwarePath,
      board::kFlashBase + board::kMarlinOffset, board::kMaximumImageSize);
  if (!image.valid) {
    LittleFS.remove(kFirmwarePath);
    setStatus(JobState::Error, 0, image.error);
    server.send(400, "text/plain", image.error);
    return;
  }
  gStatus.imageSize = image.size;
  gStatus.imageLoadAddress = image.loadAddress;
  gStatus.uploadedInitialSp = image.initialSp;
  gStatus.uploadedResetVector = image.resetVector;
  gStatus.uploadedCrc32 = image.crc32;
  gStatus.installedInitialSp = 0;
  gStatus.installedResetVector = 0;
  gStatus.installedCrc32 = 0;
  gStatus.targetDebugPortId = 0;
  gStatus.targetId = 0;
  gStatus.targetIdCode = 0;
  gStatus.targetRevision = 0;
  gStatus.flashBytes = 0;
  gStatus.targetModel = "";
  gStatus.targetUniqueId = "";
  gStatus.uploadedFirmware = image.metadata.firmwareName;
  gStatus.uploadedMachine = image.metadata.machineName;
  gStatus.uploadedBoard = image.metadata.boardName;
  gStatus.uploadedMcu = image.metadata.mcuName;
  gStatus.uploadedSourceUrl = image.metadata.sourceCodeUrl;
  gStatus.uploadedProtocol = image.metadata.protocolVersion;
  gStatus.uploadedExtruders = image.metadata.extruderCount;
  gStatus.uploadedUuid = image.metadata.uuid;
  gStatus.uploadedEvidence = "Binary structure valid: ARM Cortex-M vector table linked at 0x08002000";
  gStatus.installedFirmware = "";
  gStatus.installedMachine = "";
  gStatus.installedBoard = "";
  gStatus.installedMcu = "";
  gStatus.installedSourceUrl = "";
  gStatus.installedProtocol = "";
  gStatus.installedExtruders = "";
  gStatus.installedUuid = "";
  gStatus.comparisonComplete = false;
  gStatus.identical = false;
  setStatus(JobState::Probing, 0, "Upload valid; starting read-only board preflight");
  scheduleJob(PendingJob::Probe);
}

void configureWeb() {
#if defined(ESP32)
  const char *headerKeys[] = {"X-CSRF-Token", "X-Flash-Confirm"};
  server.collectHeaders(headerKeys, 2);
#else
  server.collectHeaders("X-CSRF-Token", "X-Flash-Confirm");
#endif
  server.on("/", HTTP_GET, [] {
    if (!authorized()) return;
    String page = FPSTR(kWebUi);
    page.replace("%CSRF%", csrfToken);
    page.replace("%BOARD%", board::kTargetName);
    page.replace("%VERSION%", APP_VERSION);
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("X-Frame-Options", "DENY");
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send(200, "text/html", page);
  });
  server.on("/api/status", HTTP_GET, sendStatus);
  server.on("/api/upload", HTTP_POST, finishUpload, handleUploadData);
  server.on("/api/flash", HTTP_POST, [] {
    if (!authorized()) return;
    if (!validCsrf()) { server.send(403, "text/plain", "Invalid request token"); return; }
    if (!server.hasHeader("X-Flash-Confirm") ||
        server.header("X-Flash-Confirm") != "FLASH SKR") {
      server.send(403, "text/plain", "Type FLASH SKR exactly to authorize flashing"); return;
    }
    if (gStatus.state != JobState::Ready || !LittleFS.exists(kFirmwarePath)) {
      server.send(409, "text/plain", "Upload and validate a firmware image first"); return;
    }
    scheduleJob(PendingJob::Flash);
  });
  server.on("/api/restore", HTTP_POST, [] {
    if (!authorized()) return;
    if (!validCsrf()) { server.send(403, "text/plain", "Invalid request token"); return; }
    if (!server.hasHeader("X-Flash-Confirm") ||
        server.header("X-Flash-Confirm") != "RESTORE SKR") {
      server.send(403, "text/plain", "Type RESTORE SKR exactly to authorize recovery"); return;
    }
    if (!LittleFS.exists(kBackupPath)) { server.send(404, "text/plain", "No backup exists"); return; }
    scheduleJob(PendingJob::Restore);
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void configureLeds() {
  const int pins[] = {board::kWifiLed, board::kFlashingLed, board::kSuccessLed, board::kErrorLed};
  for (int pin : pins) if (pin >= 0) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  if (board::kFlashButton >= 0) pinMode(board::kFlashButton, INPUT_PULLUP);
}

void writeWifiLed(bool on) {
  if (board::kWifiLed >= 0) digitalWrite(board::kWifiLed, on ? HIGH : LOW);
}

void toggleWifiLed() {
  if (board::kWifiLed >= 0) digitalWrite(board::kWifiLed, !digitalRead(board::kWifiLed));
}

String startNetwork(const String &fallbackSsid) {
#if HAS_LOCAL_WIFI_CONFIG
  const IPAddress address(local_wifi::kAddress[0], local_wifi::kAddress[1],
                          local_wifi::kAddress[2], local_wifi::kAddress[3]);
  const IPAddress gateway(local_wifi::kGateway[0], local_wifi::kGateway[1],
                          local_wifi::kGateway[2], local_wifi::kGateway[3]);
  const IPAddress subnet(local_wifi::kSubnet[0], local_wifi::kSubnet[1],
                         local_wifi::kSubnet[2], local_wifi::kSubnet[3]);
  const IPAddress dns(local_wifi::kDns[0], local_wifi::kDns[1],
                      local_wifi::kDns[2], local_wifi::kDns[3]);

  WiFi.mode(WIFI_STA);
  if (WiFi.config(address, gateway, subnet, dns)) {
    WiFi.begin(local_wifi::kSsid, local_wifi::kPassword);
    const uint32_t deadline = millis() + 15000U;
    while (WiFi.status() != WL_CONNECTED &&
           static_cast<int32_t>(deadline - millis()) > 0) {
      toggleWifiLed();
      delay(250);
    }
    if (WiFi.status() == WL_CONNECTED) {
      writeWifiLed(true);
      return "http://" + WiFi.localIP().toString();
    }
  }
  WiFi.disconnect();
#endif

  // Recovery path if local provisioning is absent, mistyped, or the router is
  // unavailable. The fallback AP and web UI share the fixed admin password.
  WiFi.mode(WIFI_AP);
  WiFi.softAP(fallbackSsid.c_str(), apPassword.c_str());
  writeWifiLed(true);
  return "http://" + WiFi.softAPIP().toString();
}
} // namespace

void setup() {
  if (board::kSerialAvailable) Serial.begin(115200);
  delay(100);
  configureLeds();
  swd.begin();
  if (!LittleFS.begin()) {
#if defined(ESP32)
    if (!LittleFS.begin(true)) {
#elif defined(ESP8266)
    // A newly fitted or fully erased flash chip has no LittleFS structures.
    // Format only the configured filesystem partition, then mount it again.
    if (!LittleFS.format() || !LittleFS.begin()) {
#endif
      setStatus(JobState::Error, 0, "LittleFS mount failed");
      return;
#if defined(ESP32) || defined(ESP8266)
    }
#endif
  }
  csrfToken = randomText(24);

#if defined(ESP32)
  const uint64_t id = ESP.getEfuseMac();
  const String suffix = String(static_cast<uint32_t>(id), HEX).substring(4);
#else
  const String suffix = String(ESP.getChipId(), HEX);
#endif
  const String ssid = "SKR-Flasher-" + suffix;
  const String webAddress = startNetwork(ssid);
  configureWeb();

  if (board::kSerialAvailable) {
    Serial.println("\nSKR Mini E3 V3.0 SWD programmer");
    Serial.println("Hardware: " + String(board::kTargetName));
#if HAS_LOCAL_WIFI_CONFIG
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Wi-Fi station SSID: " + String(local_wifi::kSsid));
    } else {
      Serial.println("Station connection failed; fallback SSID: " + ssid);
      Serial.println("Fallback Wi-Fi password: " + apPassword);
    }
#else
    Serial.println("Wi-Fi SSID: " + ssid);
    Serial.println("Wi-Fi password: " + apPassword);
#endif
    Serial.println("Web user: admin");
    Serial.println("Web password: " + apPassword);
    Serial.println("Open: " + webAddress);
  }
}

void loop() {
  server.handleClient();
#if defined(ESP8266)
  if (pendingJob != PendingJob::None) {
    const PendingJob job = pendingJob;
    pendingJob = PendingJob::None;
    delay(50);
    executeJob(job);
  }
#endif
  delay(2);
}
