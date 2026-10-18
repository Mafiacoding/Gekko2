# BETA12.31c network menu and acknowledged connection test

An empty receiver report with zero valid and zero invalid packets cannot prove
which step failed. BETA12.31b produced no launcher traffic and exposed only
coarse state in SD logs, so socket readiness, configuration and actual Windows
receipt were hard to distinguish.

Settings now includes a network page: enabled flag, four editable PC IPv4
components, port, atomic SD save/apply and an explicit test packet. No BIOS or
game is required. Pending edits are distinct from active settings. Defaults OFF;
restore defaults disables telemetry while retaining the endpoint. CPU/GX/
experiment choices are preserved. Settings changes/retries use a persistent
worker and generation checks; no network operation blocks the UI/emulation
thread. DHCP/net/socket/nonblock/thread/send failures expose phase and SDK code;
local Wii IP, active endpoint, send/drop/error counts and receipt state are live.

The fixed 1,016-byte schema remains compatible for performance packets; event 6
is a test probe, excluded from CPU/performance groups. Receiver 1.1 sends a fixed
20-byte `GK2A` ACK echoing boot and sequence. Wii only accepts exact-length ACKs
from the configured IP/port, matching current boot, current-target sequence
floor and successfully sent range. Each test/endpoint change clears the prior
receipt. ACKs cannot change emulator state or invoke commands. Receipt means
an acknowledged packet, not a persistent connection or delivery guarantee.

Receiver diagnostics now include listener/port, local IPv4 candidates, received,
invalid and filtered datagrams, ACK counts/errors and termination reason. Empty
captures provide actionable metadata. Wait for the first packet increases to ten
minutes to allow setup. Capture still stops after ten minutes or Ctrl+C and
creates one ZIP. See [German guide](../tools/windows-telemetry/README-DE.md).

## Verification and limits

Tests cover actual menu text, active/pending endpoints, socket vs ACK status,
error visibility, configuration round trips (including saved OFF endpoints),
editor bounds, malformed/stale/wrong-boot ACKs, actual host UDP probe/ACK round
trip, probe exclusion, empty capture diagnostics and all prior protocol/FIFO/
deduplication/configuration-scope tests. Existing CPU/menu/config regressions
remain. Fresh native devkitPPC build and linked PPC checks are recorded below.

No claim of an established cause for the owner's failed network attempt. Native
Windows receipt/SDK DHCP on a real Wii still require the short menu test. No
BIOS/Crash/Tekken/GX-fence or FPS fix. Owner reports remain private, no logs or
network addresses are published. The isolated Wii Limits benchmark is unchanged.
