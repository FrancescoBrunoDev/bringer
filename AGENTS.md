AGENTS
-----

This repository is an embedded Arduino/PlatformIO example (ESP32‑C6) that exposes an HTTP API and an OLED menu. This document gives quick commands for building, testing and a compact code style / agent ruleset so automated agents (and humans) can operate consistently.

- Repository root: `.`
- Example app: `examples/esp32c6-oled-menu` (active PlatformIO env: `esp32c6-devkitm-1`)

Commands
-
- Build (compile only):

```
pio run -e esp32c6-devkitm-1
```

- Build + upload (replace port):

```
pio run -e esp32c6-devkitm-1 -t upload --upload-port /dev/ttyACM0
```

- Clean build artefacts:

```
pio run -e esp32c6-devkitm-1 -t clean
```

- Serial monitor (watch logs at 115200):

```
pio device monitor -e esp32c6-devkitm-1 --port /dev/ttyACM0 -b 115200
```

- PlatformIO unit tests
- There are currently no unit tests in this repository. If you add tests, PlatformIO's test runner is used:

```
pio test -e esp32c6-devkitm-1
```

- Run a single test file
- PlatformIO supports filtering test execution by file/name. Depending on PIO version use `-f`/`--filter` to pass a path or pattern. Example:

```
pio test -e esp32c6-devkitm-1 -f test/my_test.cpp
```

If `-f` is not available on your PIO version run the full suite and scope your changes to a single test file or run the unity test file on a host environment (see PlatformIO docs).

Tooling & lint
-
- There is no repository-wide lint configuration. Recommended tooling for contributors/agents:

  - Install `clang-format` and apply a shared style (see repository convention below). Run:

  ```bash
  clang-format -i src/**/*.cpp src/**/*.h
  ```

  - Prefer `cppcheck` for static analysis on host when feasible.

Style guidelines (applies to C++/Arduino code)
-
- General philosophy: keep code small, explicit and safe for constrained devices. Prefer clarity over cleverness.

- File layout & names
  - Use lower_snake for filenames: `module_name.cpp`, `module_name.h` (current repo follows this: `server.cpp`, `ui.cpp`).
  - Public headers live next to their implementation in `src/epaper_monitor/...`.

- Includes ordering
  - Project-local headers first, grouped by module, then third‑party libs, then system headers. Example:

    #include "epaper_monitor/display/display.h"
    #include "epaper_monitor/wifi.h"
    #include <ArduinoJson.h>
    #include <Arduino.h>

- Header structure
  - Use include guards in `.h` files or `#pragma once` (either is acceptable).
  - Keep headers minimal: prefer forward declarations where possible to reduce compile time.

- Naming
  - Functions: snake_case (module_scoped): `epd_drawImageFromBitplanes`, `server_init`, `ui_next`.
  - Variables: snake_case for local/instance variables. Prefix globals with module name (e.g. `server` is a static local in `server.cpp`).
  - Types/structs: PascalCase (e.g. `DisplayConfig`).
  - Macros/constants: UPPER_SNAKE (e.g. `WEB_SERVER_PORT`, `ENABLE_PARTIAL_UPDATE`). Prefer `constexpr`/`const` over `#define` when possible.

- Formatting
  - Use 2-space indentation (follow current files).
  - Line length: prefer <= 100 characters where practical.
  - Brace style: K&R / same-line opening brace (consistent with existing code).

- Imports & dependencies
  - Prefer PlatformIO `lib_deps` / `lib_extra_dirs` (already set in `platformio.ini`).
  - Avoid adding large runtime libraries without considering RAM/flash.

- Types & memory usage
  - Be mindful of RAM constraints on ESP32. Avoid large stack allocations and unbounded dynamic allocations.
  - Prefer `StaticJsonDocument` from ArduinoJson for predictable memory use. If payload size varies, use a `DynamicJsonDocument` sized conservatively and check for allocation errors.
  - Avoid using `String` in tight loops or from ISRs; prefer fixed char buffers if performance/memory is critical. Where `String` is used (the repo uses it), validate lengths before concatenation.
  - Use `std::vector<uint8_t>` for temporary binary buffers (used currently for base64 decode) but keep sizes bounded and validate capacity.

- Concurrency & interrupts
  - ISRs must be short. Do not allocate memory or call blocking APIs from ISRs.
  - Use `volatile` for flags shared between ISR and main loop and use atomic operations if needed.

- Error handling
  - Prefer boolean return values for simple validation (e.g. `bool epd_drawImageFromBitplanes(...)`) and document error causes.
  - For HTTP endpoints: return proper HTTP codes (200 for success; 400 for client error; 500 for server error). Responses should be JSON when the endpoint normally returns JSON.
  - Log helpful serial messages (`Serial.println`) for recoverable errors and provide minimal user-facing JSON errors for API clients.

- Logging & diagnostics
  - Use `Serial` for debug output. Keep messages concise and add a prefix when useful (e.g. `HTTP:` `EPD:`).
  - Avoid printing large binary blobs to serial.

- Modules & public API surface
  - Prefix module-level functions with the module name: `epd_...`, `ui_...`, `controls_...`, `wifi_...`.
  - Keep public header APIs small and stable. Export only what other modules need.

- Tests
  - Use PlatformIO unit testing with Unity (the framework is available via PIO). Place tests under `test/` and name test files `test_*.cpp`.
  - Tests should mock hardware interactions where possible (wrap hardware access behind an interface).

Agent rules (how automated agents should operate)
-
- Make non-destructive changes only. If you must modify hardware configs or secrets, ask before committing.
- Never commit binary build artefacts (PlatformIO creates `.pio` and `build` directories — keep these out of git). Respect existing `.gitignore`.
- If creating or editing code:
  - Follow the style rules above.
  - Add or update unit tests for non-trivial logic when possible.
  - Keep changes local to one concern per commit (small, reviewable commits).

- Cursor / Copilot rules
  - No `.cursor` rules found in the repository.
  - No `.github/copilot-instructions.md` present.

Where to look
-
- Main example PlatformIO file: `platformio.ini`
- Main application entry: `src/main.ino`
- Core modules: `src/epaper_monitor/{display,server,oled,ui,controls,wifi}`

Next steps agents commonly take
-
1. Build locally: `pio run -e esp32c6-devkitm-1` to verify compilation.
2. Run serial monitor during integration tests: `pio device monitor -e esp32c6-devkitm-1 --port /dev/ttyACM0 -b 115200`.
3. Add unit tests under `test/` and run `pio test -e esp32c6-devkitm-1`.

If you want, I can also:
1) Add a `.clang-format` and run it across the codebase.
2) Add a PlatformIO unit-test scaffold (mock/stub hardware access) and an example test.
