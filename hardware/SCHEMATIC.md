# Production reference schematic (ESP32-WROOM-32)

This is an implementation-level schematic/netlist. Reference designators match
`bom.csv`. Nets with the same name are electrically common. Use a detachable,
keyed 1x5 cable; do not permanently wire the programmer into the printer.

## 1. 24 V input and 3.3 V buck

```text
J1.1 PRINTER_24V -- F1 0.75A PTC -- VIN_PROTECTED ----+---- U1.VIN
J1.2 GND ---------------------------------------------+---- U1.PGND / exposed pad
                         D1 SMBJ28A
VIN_PROTECTED ------------|<TVS>|-------------------------- GND

VIN_PROTECTED -- C1 2.2uF/100V -- GND       (at U1 VIN)
VIN_PROTECTED -- C2 220nF/100V -- GND       (at U1 VIN)
U1 = LMR36510ADDAR, 400 kHz adjustable, 65 V, 1 A
U1.EN -- R1 100k -- VIN_PROTECTED            (never floating)
U1.VCC -- C3 1uF/10V -- GND
U1.BOOT -- C4 100nF/16V -- U1.SW
U1.SW -- L1 22uH, Isat >= 1.5A, DCR <= 150mOhm -- BUCK_3V3
BUCK_3V3 -- C5,C6,C7 each 22uF/10V X7R -- GND
BUCK_3V3 -- R2 100k -- U1.FB -- R3 43.2k -- GND
U1.PG -- R4 47k -- BUCK_3V3                   (test point only)
```

The values are TI's 24 V / 3.3 V / 400 kHz application values. A 65 V converter
and 28 V standoff TVS provide substantially more printer-bus margin than common
28-32 V hobby buck parts. Keep the TVS/fuse loop away from logic ground routes.

## 2. USB-C programming power and source OR-ing

```text
J2 USB-C USB2 receptacle:
  all VBUS pins -> USB_5V; all GND/shield pins -> GND/chassis per enclosure
  A6+B6 -> USB_DP; A7+B7 -> USB_DM
  CC1 -- R5 5.1k -- GND; CC2 -- R6 5.1k -- GND
  USB_DP, USB_DM -> U2 USBLC6-2SC6 -> GND (place beside J2)

USB_5V -- F2 0.5A PTC -- U3 AP2112K-3.3.IN
U3.IN -- C8 1uF -- GND; U3.OUT -- C9 1uF -- GND
U3.OUT = USB_3V3

BUCK_3V3 -> U4 LM66100DCKR VIN; U4 CE tied BUCK_3V3; U4 VOUT --+
USB_3V3  -> U5 LM66100DCKR VIN; U5 CE tied USB_3V3;  U5 VOUT --+-- SYS_3V3
SYS_3V3 -- C10 10uF and C11 100nF -- GND
```

The two ideal-diode ICs prevent USB from driving the 24 V buck output and prevent
printer power from driving the USB LDO. Do not replace them with direct rail ties.

## 3. USB-UART and ESP32

```text
U6 CP2102N-A02-GQFN24:
  GND/pad -> GND
  VREGIN, VDD, VIO -> SYS_3V3            (internal regulator not used)
  each power pin -> 4.7uF || 100nF -> GND, placed at that pin
  RSTb -- R36 1k -- SYS_3V3
  USB_5V -- R37 22.1k -- U6.VBUS -- R38 47.5k -- GND
  D+ -> USB_DP; D- -> USB_DM
  TXD -- R7 470R --> ESP_U0RXD / U7 pin 34 (GPIO3)
  RXD <-- R8 470R --- ESP_U0TXD / U7 pin 35 (GPIO1)
  DTR, RTS: no-connect in base design

U7 ESP32-WROOM-32:
  pins 1,15,38 and exposed ground area -> solid GND plane
  pin 2 3V3 -> SYS_3V3; C13 10uF + C14 100nF to GND at pin 2
  pin 3 EN -> R9 10k to SYS_3V3; C15 1uF to GND; SW2 RESET to GND
  pin 25 GPIO0 -> R10 10k to SYS_3V3; SW3 ESP_BOOT to GND
```

Manual ESP programming is deterministic: hold `ESP_BOOT`, tap `RESET`, release
`ESP_BOOT`, then upload over USB-C. If automatic download is desired, copy the
official ESP32-DevKitC V4 DTR/RTS two-transistor subcircuit exactly; do not use
two independent inverters because simultaneous DTR/RTS assertion can hold reset.

Powering the QFN24 VREGIN/VDD/VIO together from SYS_3V3 follows Silicon Labs'
"regulator not used" connection. The VBUS divider is still required so the IC
can detect USB attachment without exposing an unpowered device pin to 5 V.

## 4. Detachable SWD interface

```text
ESP GPIO21 -- R20 47R ----------------------- J3.2 SWDIO
ESP GPIO18 -- R21 47R ----------------------- J3.4 SWCLK
ESP GPIO19 -- R22 100R -- Q1 2N7002 gate
Q1 gate -- R23 100k -- GND
Q1 source -> GND; Q1 drain -- R24 100R ------ J3.5 NRST  (open drain)
GND ----------------------------------------- J3.3 GND
J3.1 VTREF -- R25 100k --+-- R26 100k -- GND
                         +-- C20 10nF -- GND
                         +-------------------- ESP GPIO34 (ADC1)

D2 TPD4E05U06DQAR channels at J3 protect SWDIO, SWCLK, NRST, VTREF to GND.
TP1=SWDIO, TP2=SWCLK, TP3=NRST, TP4=VTREF, TP5=GND on programmer side.
```

J3 is keyed and has the **same numeric order** as BTT J11:

| Programmer J3 | BTT J11 | Exact connection |
|---:|---:|---|
| 1 | 1 | VTREF sense to SKR 3.3 V; never powers SKR |
| 2 | 2 | GPIO21 through 47 ohm to SWDIO / STM32 PA13 |
| 3 | 3 | Ground |
| 4 | 4 | GPIO18 through 47 ohm to SWCLK / STM32 PA14 |
| 5 | 5 | 2N7002 open-drain output to PF2-NRST |

No level shifter is fitted: both ends are 3.3 V. TXS/TXB auto-direction parts can
fight SWDIO turnaround and should not be used. The 47 ohm resistors damp cable
edges; use a 10-15 cm cable with ground adjacent/twisted to SWDIO and SWCLK.

## 5. Controls and indicators

```text
SYS_3V3 -- R30 2.2k -- LED1 POWER (green) -- GND
ESP GPIO25 -- R31 2.2k -- LED2 WIFI (blue) -- GND
ESP GPIO26 -- R32 2.2k -- LED3 FLASHING (amber) -- GND
ESP GPIO27 -- R33 2.2k -- LED4 SUCCESS (green) -- GND
ESP GPIO33 -- R34 2.2k -- LED5 ERROR (red) -- GND
ESP GPIO32 -- R35 10k to SYS_3V3; SW1 FLASH/AUTH from GPIO32 to GND
```

All selected GPIOs avoid ESP32 boot straps and the module's SPI-flash pins.
