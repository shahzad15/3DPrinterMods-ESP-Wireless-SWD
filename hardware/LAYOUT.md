# PCB layout and assembly guidance

Use a two-layer board only if the bottom can remain an almost unbroken ground
plane. Four layers (signal / ground / 3.3 V / signal) are preferred around a
printer's motors and heaters.

1. Put J1, F1, D1, C1/C2, U1, L1, and C5-C7 together at one board edge. Minimize
   the `VIN-CIN-PGND` and `SW-L-COUT-PGND` hot loops. Keep SW copper small, with
   no SW trace or inductor under the ESP antenna, VTREF ADC, USB, or SWD.
2. Follow TI's PowerPAD copper/via pattern. Use a short Kelvin FB trace from the
   output capacitor positive node to R2/R3; keep it away from SW and L1.
3. Place the ESP module at the opposite edge with its antenna fully overhanging
   the PCB if possible. Otherwise obey Espressif's antenna keepout on every
   copper layer and keep metal/enclosure parts away.
4. Put C13/C14 directly at the WROOM 3.3 V pin. Use a wide 3.3 V feed and ground
   stitching vias. Wi-Fi current pulses are the dominant logic transient.
5. Place USBLC6 beside J2, before the D+/D- traces travel inward. Route D+/D- as
   a short 90-ohm differential pair, matched within 0.5 mm, without stubs.
6. Place D2 at J3. Put R20/R21 at the ESP source end. Route SWCLK away from the
   antenna and buck switch node with continuous ground underneath.
7. Make pin 1 unmistakable on both silkscreen and copper: square pad, triangle,
   and the text `1:3V3 SENSE`. Label all five signals, not only `SWD`.
8. Put the five SWD test pads and UART pads where pogo pins can reach them. Add
   test points for raw 24 V, BUCK_3V3, USB_3V3, SYS_3V3, EN, and GPIO0.
9. Use a keyed housing and strain relief. The recommended field workflow is to
   leave only a small header accessible on the printer and plug the programmer
   in when needed.
10. Keep the programmer in an insulated enclosure. The printer's 24 V input is
    not SELV-safe by assumption; confirm the actual PSU and protective earth.

