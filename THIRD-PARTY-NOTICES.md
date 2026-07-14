<!-- SPDX-FileCopyrightText: 2026 Benoit Quiniou -->
<!-- SPDX-License-Identifier: MIT -->

# Third-Party Notices

VBAN-Key itself is MIT-licensed (see [LICENSE](LICENSE)). It vendors the
components below, each under its own license. The full license text for each is
kept next to its sources, and each vendored tree carries a `.clang-format` /
`.clang-tidy` that excludes it from this project's formatting and linting so the
upstream code stays byte-for-byte intact.

## tomlc17

- **Purpose:** TOML parser for the runtime configuration (the `config` component). **Compiled into the firmware.**
- **Upstream:** https://github.com/cktan/tomlc17
- **Vendored at:** `components/tomlc17/` — `tomlc17.c`, `tomlc17.h`
- **License:** MIT — [`components/tomlc17/LICENSE`](components/tomlc17/LICENSE)
- **Copyright:** © 2024–2026 CK Tan
- **Updating:** replace `tomlc17.c` / `tomlc17.h` from upstream `src/`; keep `LICENSE`.

## Unity

- **Purpose:** C unit-test framework for the component and simulator test suites (built via `host/`). **Host tests only — not part of the firmware.**
- **Upstream:** https://github.com/ThrowTheSwitch/Unity
- **Vendored at:** `third_party/unity/` — `unity.c`, `unity.h`, `unity_internals.h`
- **License:** MIT — [`third_party/unity/LICENSE.txt`](third_party/unity/LICENSE.txt)
- **Copyright:** © 2007–2026 Mike Karlesky, Mark VanderVoord, & Greg Williams
- **Updating:** replace the three files from upstream `src/`; keep `LICENSE.txt`.

Both are used unmodified and retain their MIT copyright and permission notices as
their licenses require.
