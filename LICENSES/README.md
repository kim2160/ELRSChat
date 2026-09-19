# License inventory

The root [LICENSE](../LICENSE) is the original ExpressLRS GPLv3 text.
Do not remove per-file copyright, license, or warranty notices. The additional
ELRSChat code is identified in [NOTICE.md](../NOTICE.md).

## Code present in this source tree

| Component | License / retained notice |
|---|---|
| ExpressLRS firmware | Root GPLv3; individual files retain any separate terms |
| ELRSChat additions | GPL-3.0-or-later, as scoped in NOTICE.md |
| Original `src/lua/elrs.lua` | OpenTX copyright and GPLv2 header; [GPLv2 full text](GPL-2.0.txt) |
| `src/lib/FEC` | Existing `COPYING` and `COPYING.LESSER` retained |
| `src/lib/SX127xDriver` | Existing MIT notice in `license.txt` retained |
| `src/lib/SX1280Driver/SX1280_hal.*` | Semtech 2015 copyright and Revised BSD notices retained; [BSD-3-Clause reference text](BSD-3-Clause.txt) |
| Semtech LR1121 headers | Complete Clear BSD notices retained in the affected headers |
| `src/python/external/esptool` | Existing LICENSE and source notices retained |
| `src/python/external/inputimeout` | Existing LICENSE and source notices retained |

This index does not replace notices elsewhere in the upstream tree. A license
mentioned here does not override a more specific notice in a source file.
The generic BSD text is a reference for the named license; its template
copyright line does not replace the actual copyright notices in the source.

## External build dependencies

PlatformIO and the upstream build files fetch dependencies independently.
Their code and binary packages are not vendored in this repository. Retain their
notices when building or redistributing a combined result. Examples from the
pinned build configuration include:

- AsyncTCP and ESPAsyncWebServer: LGPL-3.0.
- Arduino ESP32 core: LGPL-2.1-or-later; bundled SDK components have their own terms.
- NimBLE-Arduino: Apache-2.0, with NOTICE files for it and its components.
- ArduinoJson and ESP32-BLE-Gamepad: MIT.
- U8g2: BSD-2-Clause for library code; individual fonts have separate terms.
- Arduino_GFX and generated MAVLink headers: follow the notices in the fetched source.

Hardware definitions come from the separate
[ExpressLRS/targets](https://github.com/ExpressLRS/targets) repository at commit
`2b90c511e965304e333d99ca5f724dbc8b7c154f`. They are fetched into an ignored local
directory, not republished or relabeled as GPL by ELRSChat. The absence of a
standalone license file in that snapshot is not a grant to relicense it.

A future binary distribution must separately prepare its corresponding-source
access and applicable third-party notices. This source-only publication does
not present a completed license audit of every possible hardware/SDK build.

## Full-text provenance

- `GPL-2.0.txt`: https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
- `BSD-3-Clause.txt`: https://github.com/spdx/license-list-data/blob/main/text/BSD-3-Clause.txt
- Retrieved 2026-09-19. Upstream license files remain in their original locations.
