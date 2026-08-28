# 📡 Wireless SWD Firmware Flasher & Hardware Bridge for 3D Printer Mainboards

Official open-source repository for **Wireless SWD Firmware Flashing & Remote Unbricking over Wi-Fi** using an ESP32 / ESP8266 microcontroller module paired with BigTreeTech SKR 3 (STM32H723) and SKR Mini E3 V3.0 (STM32G0B1RE) 3D printer motherboards.

[![3D Printer Mods Banner](https://3dprintermods.xyz/wp-content/uploads/2026/08/swd_wireless_esp32_diagram.jpg)](https://3dprintermods.xyz/2026/08/28/wireless-swd-firmware-updates-unbricking-over-wi-fi-esp32-bridge-guide/)

## 🌐 Complete Hardware & Setup Documentation
* 📖 **Step-by-Step Article & Wiring Guide**: [https://3dprintermods.xyz/2026/08/28/wireless-swd-firmware-updates-unbricking-over-wi-fi-esp32-bridge-guide/](https://3dprintermods.xyz/2026/08/28/wireless-swd-firmware-updates-unbricking-over-wi-fi-esp32-bridge-guide/)
* 🔌 **5-Wire SWD RPi Bitbang Guide**: [https://3dprintermods.xyz/2026/08/28/5-wire-swd-rpi-bitbang-flashing-guide-unbrick-bigtreetech-skr-mainboards/](https://3dprintermods.xyz/2026/08/28/5-wire-swd-rpi-bitbang-flashing-guide-unbrick-bigtreetech-skr-mainboards/)
* 🏠 **3D Printer Mods Lab**: [https://3dprintermods.xyz](https://3dprintermods.xyz)

## 📌 Features
* **Drag-and-Drop Web Browser Portal (`http://192.168.1.x`)**: Upload `bootloader.bin`, `marlin.bin`, or `klipper.bin` over Wi-Fi.
* **Remote Network GDB Server (Port 2331)**: Debug ARM Cortex microcontrollers wirelessly.
* **Hardware Wiring Pinout**:
  * `GPIO 21` -> `SWDIO`
  * `GPIO 22` -> `SWCLK`
  * `GPIO 19` -> `NRST`
  * `GND` -> `GND`
  * `3V3` -> `3.3V VCC`

## 🛒 Recommended Hardware Deals
* [ESP32 Modules & Wiring Gear on AliExpress](https://s.click.aliexpress.com/e/_c3KxC2ij)
* [BigTreeTech SKR Mainboards on AliExpress](https://s.click.aliexpress.com/e/_c4lyGrWX)
