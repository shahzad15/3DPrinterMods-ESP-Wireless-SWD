"""ESP32/ESP8266 MicroPython keyboard-matrix scanner and serial monitor.

The program detects the board and selects its configured wiring map.
Change BOARD_PIN_MAP below if your keyboard is wired differently.

Open the ESP32 serial console at 115200 baud to see press/release events.
No external pull-up resistors are required because the column inputs use the
ESP32's internal pull-ups.
"""

import time
import sys

try:
    from machine import Pin
except ImportError:
    raise SystemExit(
        "This file is ESP MicroPython firmware and cannot run with the "
        "Windows .venv interpreter. Upload it to COM9 with:\n\n"
        "  .venv\\Scripts\\python.exe -m mpremote connect COM9 fs cp main.py :main.py\n"
        "  .venv\\Scripts\\python.exe -m mpremote connect COM9 reset\n\n"
        "The board must have MicroPython firmware installed first."
    )


# Matrix wiring. COM9 was hardware-detected as an ESP8266EX. The connected
# switch uses NodeMCU D1 (GPIO5) and D2 (GPIO4), so D1 is scanned as row 0 and
# D2 is read as column 0.
BOARD_PIN_MAP = {
    "esp8266": {
        "rows": (5,),     # NodeMCU D1
        "columns": (4,),  # NodeMCU D2
    },
    "esp32": {
        "rows": (16, 17, 18, 19),
        "columns": (21, 22, 23, 25),
    },
}

if sys.platform not in BOARD_PIN_MAP:
    raise RuntimeError("Unsupported MicroPython platform: {}".format(sys.platform))

ROW_PINS = BOARD_PIN_MAP[sys.platform]["rows"]
COL_PINS = BOARD_PIN_MAP[sys.platform]["columns"]

# A state must remain unchanged this long before it is reported. This removes
# the rapid on/off transitions produced by mechanical switch bounce.
DEBOUNCE_MS = 25
SCAN_INTERVAL_MS = 2


rows = [Pin(gpio, Pin.OUT, value=1) for gpio in ROW_PINS]
columns = [Pin(gpio, Pin.IN, Pin.PULL_UP) for gpio in COL_PINS]


def scan_matrix():
    """Return a set containing every currently pressed (row, column) pair."""
    pressed = set()

    for row_number, row_pin in enumerate(rows):
        # A pressed switch connects this LOW row to its pulled-up column.
        row_pin.value(0)
        time.sleep_us(30)  # Let the GPIO signals settle before reading.

        for column_number, column_pin in enumerate(columns):
            if column_pin.value() == 0:
                pressed.add((row_number, column_number))

        row_pin.value(1)

    return pressed


def print_event(event, key):
    """Print one easy-to-read line for a serial data monitor."""
    row_number, column_number = key
    print(
        "{} | row={} (GPIO{}) | column={} (GPIO{})".format(
            event,
            row_number,
            ROW_PINS[row_number],
            column_number,
            COL_PINS[column_number],
        )
    )


def run():
    print("\nESP keyboard matrix serial data monitor")
    print("Board detected: {}".format(sys.platform))
    print("Matrix: {} rows x {} columns".format(len(rows), len(columns)))
    print("Rows: {}".format(ROW_PINS))
    print("Columns: {}".format(COL_PINS))
    print("Waiting for key activity...\n")

    stable_keys = set()
    candidate_keys = scan_matrix()
    candidate_since = time.ticks_ms()

    while True:
        scanned_keys = scan_matrix()
        now = time.ticks_ms()

        # Restart the debounce timer whenever the raw scan changes.
        if scanned_keys != candidate_keys:
            candidate_keys = scanned_keys
            candidate_since = now

        elif (
            candidate_keys != stable_keys
            and time.ticks_diff(now, candidate_since) >= DEBOUNCE_MS
        ):
            for key in sorted(candidate_keys - stable_keys):
                print_event("PRESSED ", key)

            for key in sorted(stable_keys - candidate_keys):
                print_event("RELEASED", key)

            stable_keys = candidate_keys.copy()

        time.sleep_ms(SCAN_INTERVAL_MS)


try:
    run()
except KeyboardInterrupt:
    # Leave every row inactive when stopping from the MicroPython REPL.
    for row in rows:
        row.value(1)
    print("\nMatrix scanner stopped.")
