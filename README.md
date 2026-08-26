# VBAN-Key

Minimalist firmware that sends [VBAN](https://vb-audio.com/Voicemeeter/VBANProtocol_Specifications.pdf)
UDP commands (TEXT / MIDI) to Voicemeeter peers on key-press / GPI events.

- **Target:** ESP32-C3 (SuperMini), Wi-Fi station mode
- **Framework:** ESP-IDF, C99
- **License:** MIT — see [LICENSE](LICENSE)

## Layout

- `components/` — portable, pure-C99 core (no ESP-IDF deps, host-testable)
  - `vban/` — VBAN wire-format header + packet builders (header-only)
  - `common/` — shared enums + a small bitset helper
  - `config/` — TOML config → model (parsed with the vendored tomlc17)
  - `vban_script/` — `SendText()` / `SendMidi()` / `Wait()` parser + executor
  - `button/` — debounce + edge + mode state machine
  - `runtime/` — live buttons: sense → FSM → dispatch
  - `vban_net/` — VBAN packet assembly + UDP send (shared host / ESP-IDF)
  - `tomlc17/` — vendored TOML parser (third party)
- `main/` — app entry + the ESP-IDF platform shim (`platform/`)
- `data/` — runtime config flashed to the LittleFS `storage` partition
- `host/` — standalone native build (Ninja), with the Unity shim (`host/unity_shim/`)
  and interactive simulator (`host/sim/`)
- `components/<comp>/test/` — component Unity tests
- `host/sim/test/` — simulator Unity tests
- `third_party/unity/` — vendored Unity test framework (third party)

## Third-party dependencies

Two components are vendored, both MIT-licensed — full attribution in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md):

- **tomlc17** (`components/tomlc17/`) — the TOML config parser, compiled into the firmware.
- **Unity** (`third_party/unity/`) — the C unit-test framework, used only by the host tests.

Each vendored tree keeps its own `LICENSE` and is excluded from this project's
formatting/linting so it stays byte-for-byte upstream.

## Prerequisites

- **Host tests** (`make host`, `make sim`) need only CMake, Ninja, and a C compiler — no ESP-IDF.
- **Device firmware** (`make build`, `make flash`), target linting (`idf.py clang-check`), and the
  ESP-IDF host-test lane (`make host-idf`)
  need ESP-IDF **v5.5.x**, installed and activated once per shell:

      mkdir -p ~/esp && cd ~/esp
      git clone -b v5.5.4 --recursive https://github.com/espressif/esp-idf.git
      cd esp-idf && ./install.sh esp32c3
      . ~/esp/esp-idf/export.sh

      ./tools/idf_tools.py install esp-clang esp-clang-libs
      . ~/esp/esp-idf/export.sh

  The `export.sh` step puts `idf.py` and the RISC-V toolchain on your `PATH`, and
  must be re-run **once in every new shell** — otherwise `make set-target` (and the
  other device targets) fail with `idf.py: No such file or directory`. To avoid
  re-typing the full path, add a convenience alias to your shell profile
  (`~/.zprofile`, `~/.zshrc`, `~/.bashrc`, …):

      alias get_idf='. $HOME/esp/esp-idf/export.sh'

  then just run `get_idf` before building.

  The `joltwallet/littlefs` dependency is fetched automatically on the first build.

## Build (device)

    make set-target
    make build
    make flash

## Test (host)

    make host        # native Unity suites (CMake + Ninja + CTest)
    make host-idf    # the same suites, compiled and run through ESP-IDF's Linux target

`make test` is an alias for `make host`.

## Lint (device)

Activate ESP-IDF and use its Clang toolchain for ESP32-C3-specific code:

    . ~/esp/esp-idf/export.sh
    IDF_TOOLCHAIN=clang idf.py -B build/idf-clang \
      -D SDKCONFIG=build/idf-clang/sdkconfig clang-check --exit-code

## Simulator (host)

    make sim
    make demo

## TODO

- **Continuous integration** — GitHub Actions running the build + test lanes (native host, ESP-IDF Linux, firmware, on-chip build) on every push.
- **On-chip real-world test** — flash an ESP32-C3 and validate end-to-end against a live Voicemeeter instance.
- **Wiki** — illustrated guide and step-by-step instructions to build a VBAN keypad (wiring, enclosure, flashing, configuration).
