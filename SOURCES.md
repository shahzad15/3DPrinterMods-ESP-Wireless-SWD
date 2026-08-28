# Primary references

The design was checked against these primary sources on 2026-08-07:

- BIGTREETECH official repository, V3.0 schematic:
  <https://github.com/bigtreetech/BIGTREETECH-SKR-mini-E3/blob/master/hardware/BTT%20SKR%20MINI%20E3%20V3.0/Hardware/BTT%20E3%20SKR%20MINI%20V3.0_SCH.pdf>
- BIGTREETECH official repository, V3.0 pin drawing:
  <https://github.com/bigtreetech/BIGTREETECH-SKR-mini-E3/blob/master/hardware/BTT%20SKR%20MINI%20E3%20V3.0/Hardware/BTT%20E3%20SKR%20MINI%20V3.0_PIN.pdf>
- Marlin current STM32G0 build configuration:
  <https://github.com/MarlinFirmware/Marlin/blob/bugfix-2.1.x/ini/stm32g0.ini>
- ST STM32G0x1 reference manual RM0444:
  <https://www.st.com/resource/en/reference_manual/rm0444-stm32g0x1-advanced-armbased-32bit-mcus-stmicroelectronics.pdf>
- ST STM32G0B1 data sheet:
  <https://www.st.com/resource/en/datasheet/stm32g0b1ke.pdf>
- ST STM32G0 hardware-development application note AN5096:
  <https://www.st.com/resource/en/application_note/an5096-getting-started-with-stm32g0-series-hardware-development-stmicroelectronics.pdf>
- TI LMR36510 data sheet:
  <https://www.ti.com/lit/ds/symlink/lmr36510.pdf>
- Espressif ESP32-WROOM-32 datasheet:
  <https://documentation.espressif.com/esp32-wroom-32_datasheet_en.pdf>
- Espressif ESP32-DevKitC V4 reference schematic:
  <https://dl.espressif.com/dl/schematics/esp32_devkitc_v4_sch.pdf>
- Silicon Labs CP2102N data sheet:
  <https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf>
- Alex Taradov embedded SWD reference (BSD-3-Clause):
  <https://github.com/ataradov/embedded-swd>
- Alex Taradov EDBG STM32G0 flash target (BSD-3-Clause):
  <https://github.com/ataradov/edbg/blob/master/target_st_stm32g0.c>
- OpenOCD STM32G0B/G0C device ID definition (`0x467`):
  <https://openocd.org/doc/doxygen/html/stm32l4x_8h.html>

The repository also contains local copies of the two BTT PDFs so connector
details remain reviewable if upstream paths change.
