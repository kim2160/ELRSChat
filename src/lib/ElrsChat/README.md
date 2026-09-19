# Optional transmitter peer messaging

This is an experimental, explicitly entered RF mode for short public messages.
It does not run concurrently with RC, and it does not add RF acknowledgements,
automatic retries, or mesh forwarding. The default boot mode is ordinary RC.

## Local interface

The TX registers one hidden CRSF STRING parameter named `ELRS Chat`. Its field
number is assigned by the existing parameter registry, not hardcoded. The Lua
tool discovers it through device information (0x29) and parameter reads (0x2C).
Writes use 0x2D and receive the standard accepted-value echo. The application
later replaces the volatile string with its response, read as 0x2B chunks.
Each multi-chunk read uses an immutable snapshot. The STRING maximum length is
56 characters. Values are canonical base64 representations of local CH/v2
messages; the initial value is `v1` to identify this transport revision.

Outside chat mode, only requests addressed to the TX, from the Lua endpoint,
for this registered field are intercepted. During chat, other extended commands
are blocked except model selection, which cancels chat and reaches the normal
model handler. Raw RC channel frames keep their upstream routing in both modes.
0x7F is not used or intercepted. Standard `elrs.lua` does not need changes.

## Lifetime and RF ownership

Lua r13 sends integer frequency in kHz (uint32 little-endian) and the ELRS power
enum as a five-byte Enter body. Empty Enter retains the current chat settings
(initially the compiled defaults) for older scripts. The adapter validates the requested band and calibrated power
before suspending RC. Settings are temporary RAM state; ordinary RC settings
and persistent configuration are unchanged. A live session cannot be retuned.
After exiting, the next entry can select another channel; pending records from
the previous channel are discarded. Lua verifies the applied frequency bytes
and power before enabling its send UI. Existing experimental-RF/domain gates
remain in force regardless of the values requested by Lua.

The engine, four command slots and four RF event slots are allocated with
`nothrow` on the first syntactically valid write. Reading/discovering the field
does not allocate them. The handset callbacks and loop own this state on the
same core; only RF event access crosses an ISR, under interrupt exclusion.
TX completion has a separate flag, so a full receive queue cannot discard it.
Idle application state is released six seconds after the last accepted write,
once RF mode has ended. Every new allocation obtains a new RF session nonce.

RC mode changes execute outside device callbacks. The device scheduler pauses
at callback boundaries, with request-generation acknowledgements and a bounded
100 ms wait. Failure leaves RC running. The inactive scheduler uses a lock-free
load rather than an RTOS mutex. Timer resume follows complete RC RF setup.

## Build and review boundaries

`ELRS_CHAT` is absent in stock targets. All modifications to existing C++ files
are guarded by it; the stock PlatformIO configuration and SetRFLinkRate API are
unchanged. The library lives inside `src/lib` and the native tests inside
`src/test/test_elrs_chat`. Run the existing native test workflow to include them.

Code inclusion does not authorize a new waveform under an existing RC regulatory
profile. Fixed-channel transmission additionally requires `CHAT_EXPERIMENTAL_RF`
and an explicit nonzero frequency. EU CE 2.4 GHz is refused because the RSSI
heuristic is not compliant LBT. Default firmware has no provisioned chat RF
profile. Regional RF qualification remains required before general release.

The current adapters cover SX128x, SX127x and LR1121 single-radio hardware.
Profile IDs identify the intended over-air band/modulation, not the chip family;
cross-chip interoperability still requires hardware testing. Dual-radio/Gemini
entry is rejected. New drivers and MCU families require separate validation.

Lua r13 needs firmware with configured-entry support. The previous private-frame
transport is intentionally not supported, because retaining it would retain the
CRSF namespace conflict. The RF message format remains version 1.
