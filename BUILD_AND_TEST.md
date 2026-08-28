# Build, provisioning, and test procedure

## Build firmware

Requirements: Python 3 and PlatformIO Core. From this directory:

```powershell
.\.venv\Scripts\python.exe -m pip install platformio
.\.venv\Scripts\python.exe -m platformio run -e esp8266_prototype
.\.venv\Scripts\python.exe -m platformio run -e esp8266_esp01_4m
.\.venv\Scripts\python.exe -m platformio run -e esp32_production
```

To upload the compact 4 MB module, disconnect its SKR/J11 cable, hold IO0 to
GND, tap RST, start the following command, and release IO0 when connection
begins. Add the current COM port with `--upload-port COMx` if auto-detection is
ambiguous:

```powershell
.\.venv\Scripts\python.exe -m platformio run -e esp8266_esp01_4m -t upload
```

For the ESP8266 trial, wire a NodeMCU as listed in `BoardConfig.hpp`: D2 SWDIO,
D1 SWCLK, D6 to the 2N7002 reset gate, A0 to the 10k-series/100k-pulldown VTREF sense, D5
FLASH button, D7 status LED, and common ground. Do not omit the reset FET or
connect A0 directly to the SKR 3.3 V rail.

For production, enter ESP32 download mode (hold ESP_BOOT, tap RESET, release
ESP_BOOT), then:

```powershell
.\.venv\Scripts\python.exe -m platformio run -e esp32_production -t upload
```

Open the 115200-baud console. This local installation uses the fixed web and
fallback-AP password `myadminpass` with the web username `admin`.
If `include/WifiSecrets.hpp` is present, it first attempts that configured LAN
for 15 seconds and prints its static web address. Otherwise—or if association
fails—it creates the printed `SKR-Flasher-*` fallback AP at
`http://192.168.4.1`. Log in as `admin` with the fixed web password.

## Hardware bring-up before connecting an SKR

1. Inspect polarity, QFN/PowerPAD solder, shorts, and J3 pin numbering.
2. Power from a current-limited 24 V bench supply: start at 50 mA limit, then
   raise to 500 mA after confirming no fault. Verify BUCK_3V3 and SYS_3V3 are
   3.20-3.40 V. Check idle input current and buck temperature for 10 minutes.
3. Power from USB-C only. Verify USB_3V3 and SYS_3V3, CP2102 enumeration, serial
   output, reset/boot buttons, AP creation, and all LEDs.
4. Apply USB and 24 V together. Confirm neither source backfeeds the other:
   USB VBUS must not appear at J1, and BUCK_3V3 must not rise when only USB is
   present upstream of U4. Remove each source in turn without ESP resets.
5. Inject a 3.3 V laboratory source into J3 pin 1 only. Confirm the UI/serial
   VTREF estimate is near 3.3 V and that J3 pin 1 sources less than 50 uA.
6. With J3 disconnected, verify SWDIO/SWCLK are high impedance at boot and Q1
   is off. Press FLASH and confirm GPIO32 goes low.

## Non-destructive SWD test

1. Power the printer normally. Disable heaters and motors; park the tool safely.
2. With both units off, attach the keyed 10-15 cm cable. Continuity-check J3.3
   to BTT ground and J3.1 to the BTT 3.3 V test point. Never power the SKR from
   J3.1.
3. Power the printer, then the programmer. Upload a known correct Marlin binary
   built as `STM32G0B1RE_btt`. Upload must automatically start read-only
   preflight; do not authorize Flash yet. The UI must accept its vector table,
   report the controller and comparison, and show Ready only when compatible.
   Confirm it displays the live SWD DP/device/revision IDs, flash size, UID, and
   the uploaded size/load/SP/reset/CRC32 fields. Embedded Marlin identity fields
   must be labeled as metadata rather than treated as guaranteed facts.
4. For the first live test, attach a logic analyzer to TP1/TP2/TP3 before the
   upload. During preflight confirm NRST low during connection, SWCLK at roughly
   0.1-0.3 MHz, SWDIO turnarounds without contention, detected ID 0x467, and
   reported 256 or 512 KiB flash. No flash unlock or erase command may occur.

## Programming and recovery test

1. Save the current SD-card firmware separately. Keep an ST-Link available for
   initial validation; this custom programmer cannot be considered proven until
   tested on sacrificial hardware.
2. Perform a full backup/program/verify cycle. Do not interrupt power. The UI
   should finish Success and Marlin should boot. Confirm version, USB serial,
   endstops, thermistors, fans, heaters (cold/controlled test), and each motor.
3. Before the first erase, confirm preflight reports device ID `0x467`, a 256 or
   512 KiB target, a stable 24-character unique ID, and a different image.
   Compare the unique ID and capacity with STM32CubeProgrammer or ST-Link.
4. Upload that same image again after a successful update. It must report an
   exact byte match, identify it as already installed, and keep Flash disabled.
5. If the images embed M115/board strings, compare the installed and uploaded
   names. Also test an image without them; the UI must say the metadata was not
   embedded instead of inventing a printer identity.
6. Repeat at least 20 times with two known firmware images. Compare an external
   ST-Link readback of `0x08002000..image_end` to the uploaded file.
7. Test validation failures: random file, zero-byte file, image linked at
   0x08000000, oversized image, target unplugged, an image with an explicitly
   embedded wrong board or MCU marker, and a wrong confirmation phrase.
   None may erase target flash.
8. Test recovery on a sacrificial board by deliberately corrupting one uploaded
   byte after backup (temporary test build). Verification must fail and the old
   application pages must be restored and verified.
9. Only after these tests, fit the enclosure and use the field cable. Leave the
   SKR header exposed and keep the programmer unplugged during normal printing.

There is no substitute for a hardware test on the actual board. Successful host
compilation verifies source portability, not SWD signal integrity or flash
controller behavior on a particular BTT production variant.
