# Build a VBAN-Key hardware prototype

This guide explains how to build a VBAN-Key prototype using the same
microcontroller board as the original project.

## Microcontroller board

The reference prototype uses this board:

- Product reference: **K93-ESP32-C3-SuperMini**
- Manufacturer: **Shenzhen Esida Electronics Co., Ltd**
- Board markings: **TENSTAR ROBOT**, **ESP32-C3**, **Super Mini**
- Microcontroller: **Espressif ESP32-C3**

The accessible connector labels are:

- First side: `5V`, `G`, `3.3`, `4`, `3`, `2`, `1`, `0`
- Second side: `5`, `6`, `7`, `8`, `9`, `10`, `20`, `21`

Reference documentation:

- [ESP32-C3 Series datasheet](https://documentation.espressif.com/esp32-c3_datasheet_en.pdf)
- [Espressif ESP32-C3 GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html)
- [Espressif ESP32-C3 hardware design guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c3/schematic-checklist.html)

## GPIO selection

Connect each normally-open push button between its GPIO pin and `G` (ground).
The firmware configures the GPIO input with its internal pull-up resistor.

### Recommended GPIOs

Use these pins for buttons:

| GPIO | Note |
| ---: | --- |
| 0 | Safe button input. |
| 1 | Safe button input. |
| 3 | Safe button input. |
| 4 | Safe when JTAG is not required. |
| 5 | Safe when JTAG is not required. |
| 6 | Safe when JTAG is not required. |
| 7 | Safe when JTAG is not required. |
| 10 | Safe button input. |
| 20 | Safe when UART0 RX is not required. |
| 21 | Safe when UART0 TX is not required. |

This provides up to **10 straightforward button inputs**. The prototype uses
native USB for flashing and logging, so UART0 and JTAG are not reserved.

### GPIOs to avoid

| GPIO | Reason |
| ---: | --- |
| 2 | Startup strapping pin; Espressif recommends keeping it high at boot. |
| 8 | Startup strapping pin and connected to the board's blue LED. |
| 9 | Startup strapping pin and connected to the BOOT button. Pulling it low at reset enters download mode. |

GPIO8 can be used after careful evaluation of the onboard LED circuit, but it
is deliberately excluded from the simple, repeatable prototype described here.

## Validate the GPIO input

Before assembling the complete prototype, use the dedicated GPIO test firmware
to verify the board, toolchain, USB connection, and one active-low input.

Connect the board to the computer with a data-capable USB cable. Do not connect
GPIO4 to `5V` or `3.3V`.

Build and flash the test firmware:

```sh
. ~/esp/esp-idf/export.sh
make gpio-test-build
make gpio-test-flash
```

Open the serial monitor:

```sh
make gpio-test-monitor
```

The initial output should include:

```text
gpio-test: GPIO4 ready: connect a normally-open button between GPIO4 and G
gpio-test: GPIO4 initial state: released
```

Briefly connect the pin labelled `4` to `G`, then disconnect it. The monitor
should report both transitions:

```text
gpio-test: GPIO4 pressed
gpio-test: GPIO4 released
```

The [complete output from a successful test](doc/assets/GPI4TestMonitorOutput.txt)
is available for comparison.

Exit the monitor with `Ctrl+]`.

This test overwrites the firmware currently installed on the board. It validates
the GPIO platform implementation used by the real firmware; it is not intended
to become a multi-GPIO application.
