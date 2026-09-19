# ELRSChat notices and modification record

ELRSChat is an independent experimental chat fork of ExpressLRS, maintained in
https://github.com/kim2160/ELRSChat. It is not an official ExpressLRS release and
does not claim endorsement by ExpressLRS, radio manufacturers, or Semtech.

## Upstream

- Project: ExpressLRS, developed by ExpressLRS LLC and its contributors.
- Source: https://github.com/ExpressLRS/ExpressLRS
- Base: 4.1.0, commit `a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6`.
- The original history, [GPLv3 license](LICENSE), existing copyright notices,
  per-file license terms, and warranty disclaimers are retained.
- The original project introduction is preserved in [README.upstream.md](README.upstream.md).

## ELRSChat modifications — 2026-09-19

The additions implement explicit entry into a public TX-to-TX chat mode,
temporary RF configuration, bounded session ownership, return to normal RC,
a device-local CRSF parameter interface, and an adaptive EdgeTX Lua interface.
The modification patch is [patches/elrs-4.1.0-chat.patch](patches/elrs-4.1.0-chat.patch).

ELRSChat's original additions in `src/lib/ElrsChat/`,
`src/lib/DEVICE/DevicePause.h`, `src/test/test_elrs_chat/`,
`src/lua/ELRSChat.lua`, `src/lua/ELRSChat/`, `tools/`, and the new project
documentation are offered under **GPL-3.0-or-later**. The text of GPLv3 is in
[LICENSE](LICENSE). This statement does not relicense upstream or third-party
files or widen the license-version permissions granted by their authors.

The software is distributed without warranty, including the implied warranties
of merchantability or fitness for a particular purpose, to the extent permitted
by applicable law. See LICENSE for the complete terms.

## Separate and third-party licenses

The unmodified `src/lua/elrs.lua` carries an OpenTX copyright notice and GPLv2
notice. Its license remains intact; [GPLv2](LICENSES/GPL-2.0.txt) is included.
It is a separate script from `ELRSChat.lua`.

Other bundled components retain their individual licenses. See
[LICENSES/README.md](LICENSES/README.md) and the notices in their source files.
External build dependencies are obtained from their original projects and are
not relicensed by this repository.

## Distribution scope

This repository publishes source code, a source patch, and Lua scripts.
No compiled radio firmware, bootloader, toolchain executable, or separately
packaged release ZIP is published. Upstream workflows that build/publish
firmware or access upstream infrastructure have been moved out of the active
workflow directory. The active workflow only checks source and runs native
tests; it does not upload firmware or release assets.
