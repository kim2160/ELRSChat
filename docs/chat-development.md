# Developing ELRSChat

## Source layout

- `src/`: ExpressLRS 4.1.0 with the chat integration already applied.
- `src/lib/ElrsChat/`: packet protocol, state machine, CRSF mailbox, and radio adapter.
- `src/lib/DEVICE/DevicePause.h`: bounded synchronization for temporary RC suspension.
- `src/lua/ELRSChat.lua` and `src/lua/ELRSChat/r16/`: the EdgeTX tool.
- `src/test/test_elrs_chat/`: native protocol/mailbox/session-support tests.
- `patches/elrs-4.1.0-chat.patch`: the chat source changes against the original 4.1.0 tree.

The patch is included for review and application to a separate clean upstream
checkout. **Do not apply it again to this repository**: the changes are already
present. New code uses a registered CRSF STRING parameter; no private global
CRSF frame ID is assigned or intercepted.

## Build dependencies

Use Python 3.10 or later, Git, and PlatformIO Core. The upstream configuration
pins the MCU platforms and most library versions; the MAVLink and hardware
definition repositories are pinned to commits. Dependencies retain their own
licenses. See [LICENSES/README.md](../LICENSES/README.md).

```sh
python -m pip install platformio==6.1.19
python tools/prepare_chat.py
python tools/build_chat.py --mcu ESP32 --radio 2400 --list-boards
```

`prepare_chat.py` fetches only the pinned hardware definitions into ignored
`src/hardware/`. It refuses to replace local modifications. It does not modify
or push to any upstream repository.

## Developer builds

Stock `src/platformio.ini` does not enable chat. `build_chat.py` creates a
separate local PlatformIO configuration and builds; it never uploads or flashes.
Hardware configuration, calibrated power range, and the single-RF-chip
restriction are checked before a board-specific image is exported locally.

For example, the two internal 2.4 GHz boards used during development are:

```sh
python tools/build_chat.py --mcu ESP32 --radio 2400 --board jumper.tx_2400.t-15 --frequency 2440000000 --power-mw 25 --experimental-rf
python tools/build_chat.py --mcu ESP32 --radio 2400 --board helloradio.tx_2400.v14 --frequency 2440000000 --power-mw 25 --experimental-rf
```

Run builds sequentially: their PlatformIO environment can share an output
directory. Output goes to ignored `dist/<environment>/<board>/`, with a manifest
identifying the source and embedded hardware. An image without `--board` has no
radio-module pinout and is not an installable image for an arbitrary module.

`--experimental-rf` is an explicit opt-in to the experimental fixed-channel
waveform, not a regulatory qualification. Without it, chat RF entry remains
disabled, including when Lua requests a frequency. The EU CE 2.4 GHz guard and
unsupported dual-radio/Gemini guard remain in place. For sub-GHz builds, supply
the upstream RC `--domain` required by the helper.

The source supports additional MCU/radio families, but this does not imply they
have all been built or tested on hardware. No prebuilt module firmware is
published by this repository.

## Checks

```sh
python tools/check_chat_source.py
```

Run the native tests on a host with GCC/G++ and PlatformIO's native platform:

```sh
PLATFORMIO_BUILD_FLAGS=-DRegulatory_Domain_ISM_2400 pio test -d src -e native -f test_elrs_chat
```

In PowerShell, set the variable separately:

```powershell
$env:PLATFORMIO_BUILD_FLAGS = '-DRegulatory_Domain_ISM_2400'
pio test -d src -e native -f test_elrs_chat
```

Before publishing this source fork, the development workspace passed 117 C++/Lua
regression tests, 5 upstream native tests, T15/V14 builds, and a stock build with
chat disabled. The public CI runs the included 5 native tests and source checks;
it does not claim to reproduce that larger development regression suite.
Actual RF performance and RC restoration on a particular radio still need
hardware validation.

When changing source under `src/`, stage new files and update the review patch:

```sh
python tools/export_chat_patch.py
python tools/check_chat_source.py
```

Keep the modifications explicit, preserve original notices, and record relevant
changes and dates in NOTICE.md or a change log. Do not add automatic firmware
publishing or release assets to the source-only workflow without a separate
distribution decision.
