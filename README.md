# ELRSChat

**English** | [한국어](README.ko.md)

**An independent experimental public-chat fork of ExpressLRS.**

## ELRSChat changes at a glance

Key paths added or modified from ExpressLRS 4.1.0:

```text
ELRSChat/
├── patches/
│   └── elrs-4.1.0-chat.patch      # Complete chat source diff
├── src/
│   ├── lua/
│   │   ├── ELRSChat.lua          # EdgeTX tool entry point
│   │   └── ELRSChat/r16/         # All four supporting files required
│   │       ├── common.lua
│   │       ├── core.lua
│   │       ├── transport.lua
│   │       └── view.lua
│   ├── lib/
│   │   ├── ElrsChat/            # New chat engine, protocol, and RF adapter
│   │   ├── DEVICE/              # RC pause/resume support
│   │   ├── tx-crsf/             # Chat parameter interface
│   │   ├── LR1121Driver/        # Radio integration
│   │   ├── SX127xDriver/        # Radio integration
│   │   ├── SX1280Driver/        # Radio integration
│   │   └── SX12xxDriverCommon/  # Shared radio integration
│   ├── include/deferred.h      # Deferred-operation coordination
│   ├── src/
│   │   ├── rxtx_common.cpp      # RF mode integration
│   │   └── tx_main.cpp          # Chat entry/exit and normal RC restoration
│   └── test/test_elrs_chat/     # Native tests
├── tools/                      # Build, inspection, and source-check helpers
└── docs/chat-development.md     # Build and test guide
```

Quick links: [Build firmware](#build-firmware) · [Lua tool](src/lua/ELRSChat.lua) · [Required Lua modules](src/lua/ELRSChat/r16) · [Full source diff](patches/elrs-4.1.0-chat.patch)

## Overview

This is an independent project, with no official ExpressLRS release status or endorsement.

ELRSChat lets two or more ELRS transmitter modules exchange short public messages.
It uses modified ELRS module firmware and an EdgeTX Lua tool, with no changes to EdgeTX itself.
**The Lua script requires chat-capable module firmware; installing Lua alone is not enough.**

## What's published

- ExpressLRS **4.1.0 source and original Git history**, with the chat integration applied.
- The [review patch](patches/elrs-4.1.0-chat.patch) and [chat C++ source](src/lib/ElrsChat).
- [ELRSChat.lua](src/lua/ELRSChat.lua) and its [r16 Lua modules](src/lua/ELRSChat/r16).
- **Source code and scripts only.** No prebuilt firmware or separately packaged release ZIPs are provided.

Base commit: [`a9d4a9cb`](https://github.com/ExpressLRS/ExpressLRS/commit/a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6).
ELRSChat modifications first published: **2026-09-19**. Attribution and licensing are recorded in [NOTICE.md](NOTICE.md).

## How it works

**Normal RC control → enter chat through Lua → exit chat and return to normal RC**

The module's normal RC link is suspended while chat is active. RC control and chat do not operate simultaneously.

- Public broadcast, with no RF delivery acknowledgment, automatic retransmission, or mesh relay.
- Up to 32 bytes per message, the latest 50 sent/received messages kept during the session, and 20 preset messages.
- Text entry, long-ENTER to send, and scrollable conversation history.
- Layout adapts to screen resolution and text width, from 128×64 monochrome to color displays.
- `Tx:message` and `Rx(ID):message` labels. The four-digit display ID is a shortened device identifier.
- Lua requests the chat frequency and power; the module validates and applies them only for the chat session.

## Build firmware

No code editing or patching is needed. Use the commands below: a normal ELRS
build with the default `platformio.ini` does **not** enable chat.

### 1. Get the source

Install [Python](https://www.python.org/downloads/) (3.10 or later) and
[Git](https://git-scm.com/downloads/). On Windows, enable Python's **Add to PATH**
option. Open PowerShell on Windows, or Terminal on macOS/Linux:

```sh
git clone https://github.com/kim2160/ELRSChat.git
cd ELRSChat
python -m venv .venv
```

On macOS/Linux, use `python3` instead of `python` for that last command.
Activate the environment using the line for your system:

| System | Command |
|---|---|
| Windows PowerShell | `.\.venv\Scripts\Activate.ps1` |
| Windows Command Prompt | `.venv\Scripts\activate.bat` |
| macOS/Linux | `source .venv/bin/activate` |

If PowerShell blocks activation, use Command Prompt and its activation command.
Keep using this terminal in the `ELRSChat` folder for the following steps.

### 2. Prepare the build

```sh
python -m pip install platformio==6.1.19
python tools/prepare_chat.py
```

An internet connection is needed to download the build dependencies.

### 3. Build for your module

Run **only the command for your model**. These examples are for the internal
**2.4 GHz ELRS** modules and match the Lua defaults: **2440 MHz / 25 mW**.

**Jumper T15:**

```sh
python tools/build_chat.py --mcu ESP32 --radio 2400 --board jumper.tx_2400.t-15 --frequency 2440000000 --power-mw 25 --experimental-rf
```

**HelloRadio V14:**

```sh
python tools/build_chat.py --mcu ESP32 --radio 2400 --board helloradio.tx_2400.v14 --frequency 2440000000 --power-mw 25 --experimental-rf
```

`--experimental-rf` enables the experimental chat radio mode; keep it in the
command. The first build downloads extra packages and can take several minutes.
Build one model at a time.

<details>
<summary>Using another ELRS transmitter module?</summary>

For another **ESP32 + 2.4 GHz** module, list the available boards:

```sh
python tools/build_chat.py --mcu ESP32 --radio 2400 --list-boards
```

Replace the value after `--board` in a build command above with the exact key
shown for your module. Do not omit `--board` or choose a similar-looking model.
Internal and external modules may use different boards.

For other chip families, check your module's `firmware` entry in
`src/hardware/targets.json`: for example, `Unified_ESP32S3_2400_TX` means
`--mcu ESP32S3 --radio 2400`. Use those values when listing boards and building.
For 900 MHz/LR1121 options, see the [detailed build guide](docs/chat-development.md).
Gemini/dual-RF modules are unsupported; an available board entry does not mean
ELRSChat has been tested on that model.

</details>

### 4. Find the firmware and install Lua

After a successful build, the terminal prints `Build saved:` and the output
folder. For the examples above, use the matching file:

```text
dist/Unified_ESP32_2400_TX_CHAT/jumper.tx_2400.t-15/firmware.bin
dist/Unified_ESP32_2400_TX_CHAT/helloradio.tx_2400.v14/firmware.bin
```

The build only creates files; it does not flash your radio. Install your model's
`firmware.bin` using its normal **ELRS module** update method, such as the
[ELRS Wi-Fi update page](https://www.expresslrs.org/software/updating/wifi-updating/).
This is not an EdgeTX firmware update. Then copy the Lua files as described below.

## Install the Lua tool

Use the same files on both radios. Copy these files from the repository to each SD card:

| Repository path | SD card path |
|---|---|
| `src/lua/ELRSChat.lua` | `/SCRIPTS/TOOLS/ELRSChat.lua` |
| `src/lua/ELRSChat/r16/*.lua` | `/SCRIPTS/TOOLS/ELRSChat/r16/*.lua` |

All four supporting Lua files are required. Remove any old `ELRSChat.luac` cache before running the tool.
You can keep your existing standard `elrs.lua` configuration script if it works normally.

The default Lua settings are `2440 MHz / 25 mW`:

```lua
local CHAT_FREQUENCY_KHZ = 2440000
local CHAT_POWER_MW = 25
```

Radios using the same public channel must use the same frequency and compatible RF profiles.
Unsupported bands or power settings are rejected. The normal RC model settings are not permanently changed.

### Start and exit chat

1. Enable one module using CRSF. For the T15/V14 internal modules, set Internal RF = CRSF
   and External RF = OFF.
2. Open `ELRSChat` from Tools and press ENTER to enter chat.
3. At `PUBLIC CHAT READY`, press ENTER to choose a preset or write a message.
   Hold ENTER while writing to send. Use the wheel or +/- to scroll conversation history.
4. Press EXIT from the conversation screen and wait for the module to confirm its return to normal RC.

If Lua stops running, the module attempts to return to normal RC after the session expires.
The receiver needs time to reconnect, so check this transition before flying.

## Development and validation

See the [development, build, and test guide](docs/chat-development.md) for the source layout and commands.
The review patch is **already applied** to this repository.

Development focused on the internal 2.4 GHz modules in the Jumper T15 and HelloRadio V14.
Before publication, the development code passed 117 C++/Lua regression tests, 5 native tests,
builds for both models, and a stock build with chat disabled.
GitHub runs source checks and the 5 included native tests; it does not upload firmware.

This is an experimental implementation. Actual RF communication, RC restoration, range, and power
have not been validated on every model. Gemini/dual-RF hardware is unsupported, and compatibility
between different radio-chip families requires separate validation.

## License and attribution

- Based on [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS), by ExpressLRS LLC and its contributors.
- The original [GPLv3 LICENSE](LICENSE) and per-file copyright and license notices are preserved.
- ELRSChat's new code and Lua are offered under GPL-3.0-or-later within the [scope stated in NOTICE.md](NOTICE.md).
- Separate upstream licenses, including GPLv2 for the original `elrs.lua`, remain unchanged. See the [license inventory](LICENSES/README.md).
- The [original project README](README.upstream.md) is preserved separately.

Please use [this repository's Issues](https://github.com/kim2160/ELRSChat/issues) for questions and proposed changes.
