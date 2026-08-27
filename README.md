# VBAN-Key

Firmware for an ESP32-C3 keypad that sends
[VBAN](https://vb-audio.com/Voicemeeter/VBANProtocol_Specifications.pdf)
TEXT and MIDI commands over Wi-Fi.

- Target: ESP32-C3 SuperMini
- Framework: ESP-IDF 5.5.x, C99
- License: MIT — see [LICENSE](LICENSE)

## Building and using

### Requirements

- An ESP32-C3 SuperMini, switches, and a data-capable USB cable.
- CMake and Ninja.
- [ESP-IDF 5.5.x](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32c3/get-started/index.html)
  installed for the ESP32-C3.

Activate ESP-IDF in every shell used for device commands:

    . ~/esp/esp-idf/export.sh

See [Building a prototype](https://github.com/BQuiniou/VBAN-Key/wiki/Building-a-prototype)
for the parts and wiring.

### Find the serial port

Connect the board, then list likely serial ports:

    # macOS
    ls /dev/cu.usbmodem*

    # Linux
    ls /dev/ttyACM* /dev/ttyUSB*

On Windows, find the `COM` port under **Device Manager → Ports (COM & LPT)**.
If several ports are listed, disconnect and reconnect the board to identify it.

Pass the complete result to every device command, for example
`PORT=/dev/cu.usbmodem1101`, `PORT=/dev/ttyACM0`, or `PORT=COM3`.

### 1. Test the inputs

Build and flash the GPIO test before configuring the application:

    make gpio-test-build
    make gpio-test-flash PORT=/dev/cu.usbmodem1101
    make gpio-test-monitor PORT=/dev/cu.usbmodem1101

Press each switch and check its state in the monitor. Exit with `Ctrl+]`.

### 2. Configure and provision Wi-Fi

Create the device configuration:

    mkdir -p config/device
    cp config/examples/config.toml config/device/config.toml
    $EDITOR config/device/config.toml

Set the Wi-Fi SSID, network settings, destination, buttons, and commands. Do not
put the Wi-Fi password in this file.

Before first-time provisioning, confirm that eFuse key block 0 is unused:

    espefuse.py -p /dev/cu.usbmodem1101 summary

Then flash the provisioning firmware and store the credential:

    make wifi-provision-build
    make wifi-provision-flash PORT=/dev/cu.usbmodem1101
    make wifi-provision PORT=/dev/cu.usbmodem1101 SSID='my-network'

The password is prompted without echo and stored in encrypted NVS. The first
provisioning permanently reserves eFuse key block 0 for its encryption key.

### 3. Build and flash VBAN-Key

    make set-target
    make build
    make flash PORT=/dev/cu.usbmodem1101

`make flash` also opens the serial monitor. Exit with `Ctrl+]`.

## Contributing

### Layout

- `components/` — portable C99 application components and their tests
- `main/` — ESP32 application entry point and platform implementation
- `config/examples/` — tracked configuration examples
- `config/device/` — ignored configuration embedded in the device firmware
- `device_test/` — hardware-test firmware
- `device_tools/` and `tools/` — device and host provisioning tools
- `host/` and `host_test/` — simulator and host test applications
- `doc/assets/` — documentation media
- `third_party/` — vendored dependencies

### Tests

Native tests need CMake, Ninja, and a C compiler. ESP-IDF is not required.

    make host
    make sim
    make demo

The ESP-IDF Linux test build requires an active ESP-IDF environment:

    make host-idf

### Lint

Install ESP-IDF's Clang tools, reactivate its environment, then run:

    ./tools/idf_tools.py install esp-clang esp-clang-libs
    . ~/esp/esp-idf/export.sh

    IDF_TOOLCHAIN=clang idf.py -B build/idf-clang \
      -D SDKCONFIG=build/idf-clang/sdkconfig clang-check --exit-code

GitHub Actions also builds and tests the project with GCC, Clang, ESP-IDF Linux,
and ESP32-C3 toolchains.

### Releases

Releases follow Semantic Versioning and use signed annotated tags named
`vMAJOR.MINOR.PATCH`. ESP-IDF derives the firmware version from the Git tag.
The version in `config.toml` independently identifies the configuration format.
Releases do not include firmware binaries because they embed device-specific
configuration.

Third-party licenses and attribution are listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
