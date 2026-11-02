# VBAN-Key

Firmware for an ESP32-C3 keypad that sends
[VBAN](https://vb-audio.com/Voicemeeter/VBANProtocol_Specifications.pdf)
TEXT and MIDI commands over Wi-Fi, and drives output pins (GPO) from buttons or
from incoming VBAN TEXT commands.

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

See [Building a prototype](https://github.com/BQuiniou/VBAN-Key/wiki#building-a-prototype)
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

Create the device configuration from one of the tracked examples:

- `config/examples/voicemeter-2buttons.toml` — commented reference of every
  configuration key: two buttons, one latch and one momentary, text and MIDI
  streams
- `config/examples/voicemeter-4mutes.toml` — four buttons on GPIO4..GPIO7
  toggling VoiceMeeter strip mute, including a two-second unmute burst
- `config/examples/gpo-button-and-remote.toml` — one output driven by a button
  and one driven by incoming `GPO[n] = 0|1;` VBAN TEXT commands

Copy one of them and edit it:

    mkdir -p config/device
    cp config/examples/voicemeter-2buttons.toml config/device/config.toml
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

### Outputs (GPO)

Each `[[gpo]]` block declares an output pin:

    [[gpo]]
    id         = 1
    gpo_pin    = 10
    active_low = false   # true: the pin is driven LOW when the output is on

Every output starts off at boot. It is then driven by either source:

- A button: `gpo = <id>` in a `[[button]]` block turns the output on and off
  with the button state.
- The network: a `[[text_in]]` block accepts VBAN TEXT packets on a stream
  name, and every `GPO[<id>] = 1;` or `GPO[<id>] = 0;` statement in their
  payload drives that output. Other statements are ignored.

      [[text_in]]
      stream_name = "GPO1"
      port        = 6980            # optional, defaults to default_port
      ip_address  = "192.168.1.57"  # optional, accepted sender

A button script can drive another board's outputs with
`SendText("vban1", GPO[2] = 1;)`. A pin is used by at most one `gpi_pin` or
`gpo_pin`; the firmware refuses GPO on `GPIO2`, `GPIO8`, `GPIO9`, and
`GPIO11`–`GPIO19`. Every GPO write is logged as `GPO <id> WRITE HIGH|LOW
(GPIO<n>)`. An output pin supplies at most 40 mA: drive anything larger than an
LED, such as a relay coil, through a transistor. See
[Wiring outputs](https://github.com/BQuiniou/VBAN-Key/wiki#wire-outputs).

### Configuration format

`[global] version` identifies the configuration format. This firmware reads
versions `1.x.y` and `2.x.y`; a file without `version` is version 1, and any
other major version is refused. Version 2 renamed `[[text]]` and `[[midi]]` to
`[[text_out]]` and `[[midi_out]]`, and added `[[text_in]]`, `[[gpo]]`, and the
`[[button]]` `gpo` key. Version 1 files load unchanged, with their `[[text]]`
and `[[midi]]` sections converted; each format refuses the other's section
names.

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
