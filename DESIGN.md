# SKR Mini E3 V3.0 Wi-Fi SWD programmer design

## Decision summary

The production design uses an **ESP32-WROOM-32 and Arduino C++17**. The same
source also builds for an ESP8266 NodeMCU so the SWD and web flow can be tried
before laying out the production PCB.

Arduino was selected because the requirement starts with an ESP8266 trial and
the ESP8266/ESP32 cores provide compatible Wi-Fi, HTTP, LittleFS, and GPIO APIs.
The low-level SWD and STM32 flash code is independent C++ and can later be moved
behind an ESP-IDF GPIO HAL. ESP-IDF becomes worthwhile if TLS, signed images,
Ethernet, or remote debugging are added; it does not improve the basic SWD
algorithm enough to justify maintaining two frameworks now.

The ESP8266 build is a functional prototype, not the recommended final unit:

- its single CPU must stop servicing HTTP while it programs, so browser progress
  pauses and reconnects after the job;
- 80 KiB RAM and fewer non-strap GPIOs make four LEDs, VTREF, buttons, USB UART,
  and SWD awkward on a custom board;
- long Wi-Fi activity and the software watchdog increase timing complexity.

SWD tolerates pauses, so ESP8266 bit-banging can work at the selected slow clock.
ESP32 is still the better production target because a second scheduled task can
serve the web UI during backup/program/verify, it has ample safe pins, and the
requested ESP32-WROOM-32 hardware is straightforward.

## Confirmed target facts

The official BTT V3.0 schematic names J11 `SWD` and shows:

| J11 pin | Signal | STM32 pin / function |
|---:|---|---|
| 1 | 3.3V / VTREF | Target rail; sense only |
| 2 | SWDIO | PA13, with 10 kOhm target pull-up |
| 3 | GND | Common ground |
| 4 | SWCLK | PA14, with 10 kOhm target pull-down |
| 5 | RESET | PF2-NRST, 4.7 kOhm pull-up and 100 nF on target |

On the official top-view pin drawing, these are left-to-right `3.3V, SWDIO,
GND, SWCLK, RST` when the board text is upright and the SWD row is horizontal.
Always confirm the square pin-1 pad and continuity to ground before attaching a
cable; never rely only on cable color.

The V3.0 schematic labels U2 `STM32G0B1RCT6` (256 KiB). Marlin commonly names
the build environment `STM32G0B1RE_btt` (512 KiB), and real production variants
may differ. Firmware therefore reads the silicon flash-size register and accepts
only 256 or 512 KiB. It also requires DBGMCU device ID `0x467`.

Current Marlin `STM32G0B1RE_btt` builds use an 8 KiB bootloader offset:
`board_build.offset = 0x2000`, upload address `0x08002000`. The web programmer
therefore writes only from `0x08002000`, preserving the BTT SD-card bootloader.

## Architecture and safety model

```text
Browser --WPA2 AP + Basic auth--> Web upload --> LittleFS /firmware.bin
                                                   |
typed operation phrase -----------------------------+ authorizes erase
                                                   v
VTREF check -> SWD connect under reset -> identify/size -> page backup
    -> erase affected 2 KiB pages -> 64-bit programming -> byte verify -> reset
                                      |
                                      +-- failure --> automatic backup restore
```

There is no need to port all of OpenOCD or expose a CMSIS-DAP debugger. Those
projects solve breakpoint/debug-server and broad target support. This appliance
needs only ARM ADIv5 memory access and one STM32G0 flash controller. The compact
transport is adapted from the BSD-3-Clause `ataradov/embedded-swd` transaction
engine, and the target sequence follows STM32 RM0444 and the BSD-licensed
`ataradov/edbg` STM32G0 target.

Reliability controls:

- all target pins remain high impedance until VTREF is in the 2.7-3.6 V range;
- connect-under-reset prevents running Marlin peripherals during erase;
- exact MCU ID, flash capacity, file size, stack pointer, Thumb reset vector,
  and the `0x08002000` link address are checked;
- preflight reads the silicon revision and 96-bit unique ID, then compares the
  uploaded bytes with the installed application before Flash is enabled;
- attach/probe leaves the flash controller locked; unlock occurs only after the
  review dialog and typed authorization, immediately before programming;
- embedded Marlin firmware, machine, and board strings are displayed when
  found, but are labeled as best-effort metadata rather than proof of identity;
- no mass erase and no option-byte/RDP modification exist in the web API;
- affected pages are backed up before the first erase;
- standard 64-bit STM32G0 programming is used. Fast-row mode is deliberately
  avoided because it has strict delivery timing that Wi-Fi preemption can break;
- every uploaded byte is read back and compared; failed updates automatically
  attempt to restore the previous pages;
- an erase requires an exact operation-specific confirmation phrase, a per-boot
  CSRF token, HTTP Basic authentication, and the provisioned local Wi-Fi.

HTTP is unencrypted inside the WPA2 AP. Do not bridge this AP to a LAN or the
Internet. A product exposed to untrusted networks should use ESP-IDF HTTPS plus
signed firmware manifests and anti-rollback version policy.

## Module boundaries

- `FirmwareImage`: fail-closed validation of raw Marlin binaries.
- `HardwareAbstraction`: the only SWD/reset/VTREF GPIO implementation; replace
  this module to move the protocol to native ESP-IDF or another MCU.
- `SwdTransport`: SWD framing, retries, parity, AP/DP, block memory access.
- `Stm32G0Programmer`: target ID, reset/halt, page backup, erase, program, verify.
- `main`: Wi-Fi provisioning, authenticated web routes, jobs, recovery policy.
- `Status` / `WebUi`: LED state and polling UI progress.

## Sources checked

- BTT official V3.0 schematic and pin drawing in
  `bigtreetech/BIGTREETECH-SKR-mini-E3`, saved beside this project as the two
  `BTT_SKR_MINI_E3_V3.0_*.pdf` files.
- ST RM0444, STM32G0B1 datasheet/errata, and STM32G0 hardware application note.
- Current Marlin `ini/stm32g0.ini`.
- TI LMR36510 data sheet, especially its 24 V to 3.3 V application.
- Espressif ESP32-WROOM-32 datasheet and DevKitC V4 reference schematic.
