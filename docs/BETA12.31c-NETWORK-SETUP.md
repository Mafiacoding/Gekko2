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

## Actual validation record

Build code commit `75cbe6a`, native devkitPPC r50/GCC16.1.0/libogc3.1.0, previous
SPEED/DSI/AOT/GX/chain flags preserved. `tools/test_beta12_31c_network.py` passed
four existing CPU/menu/config suites, network C/receiver tests (including real
host UDP probe/ACK), new menu tests and real EE/VU/GX profile-work regression.
The delivered ELF passed 33 PPC wire/FIFO/ABI checks plus 49 PPC API/worker/ABI
checks. For the worker checks, SDK network/thread calls, IRQ/lazy-FPU behavior
and the timebase are modeled; no OS concurrency, Wii network delivery or timing
is claimed. Cases include thread/net-init/DHCP/socket/nonblock/send failures,
persistent-worker retry, wrong-boot/stale/wrong-IP ACK rejection, target changes,
queue full, OFF and a successful acknowledged probe.

Fresh Wii DOL/ELF section equality and MEM1 gate passed: static end
`0x815e5754`, 2,205,868 bytes remaining. Native Windows 1.1 and physical Wii
network receipt remain unverified; use the short menu probe, not a BIOS boot.

Windows-style UDP ICMP reset errors (e.g. 10054 after Wii socket closure) are
also regression-tested: the receiver continues and preserves the report instead
of exiting before ZIP creation. Other receive errors finalize a diagnostic ZIP.
Receiver scripts are host tools, independently tested after the Wii build; the
manifest records their hashes alongside separate source/build commits.
