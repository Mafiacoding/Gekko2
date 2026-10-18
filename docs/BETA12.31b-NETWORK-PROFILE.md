# BETA12.31b optional network profiling

A local Windows receiver replaces manual SD transfer for routine performance
snapshots. Factory default OFF; an explicit RFC1918 IPv4/port configuration in
`sd:/pcsx2/telemetry.cfg` enables it after app restart. No changes to CPU/GX or
experiment preferences. See [Windows setup](../tools/windows-telemetry/README-DE.md).

The emitter serializes a fixed 1,016-byte big-endian numeric packet, including
build/source identity, boot/session/epoch, options, wall-time intervals,
cumulative CPU/presentation scopes, native block/budget counters and GX timeout
counts. No guest instructions, addresses, strings, ROM content or filenames are
sent. No receive/control socket exists on the Wii. SD diagnostics remain.

Network initialization and nonblocking UDP sends run on an LWP worker (API
priority 10, below normal main thread). The emulator only fills a bounded
four-packet queue; full queues drop new packets and count losses. IRQ-disabled
sections only copy a packet/update indices or counters; no allocation, socket,
DHCP, file IO, wait or retry occurs in those sections. Missing configuration
starts no network services. Failed initialization does not block guest progress.
Network activity still consumes CPU; no zero-overhead or hardware timing claim.

Profiler epochs advance with the existing system_profile_reset at each run/
resume. Receiver deduplicates and orders cumulative totals by sequence, separates
all configurations/epochs, reports gaps and errors, and creates one ZIP after
ten minutes or Ctrl+C. Read-only Python standard library; Windows CMD launchers.
No automatic upload or persistent ChatGPT connection. Ordinary telemetry does
not replace detailed GX command/fault logs or demonstrate graphical correctness.

## Verification

`python3 tests/test_network_receiver.py` exercises actual C packet encoding,
strict/default-off configuration, 64-bit endianness, FIFO wrap/full/empty,
malformed packets, duplicate/reordered/lost packets, cumulative totals and
configuration/epoch grouping. A real host UDP loopback run exercises the CLI,
JSONL, report ZIP/CRC and SD setup with preserved CPU settings. All fixtures are
synthetic. `python3 tools/test_beta12_31a_status.py` retains the four menu/config/
analyzer regressions. Fresh devkitPPC build uses all previous SPEED/DSI/AOT/GX
and chain flags; only the source-commit identity macro is added.

Actual Wii network delivery, Windows-native launch/firewall behavior and overhead
remain hardware/platform checks; Linux loopback and cross-compilation do not
prove them. No BIOS/Crash/Tekken graphical fix or FPS improvement in this batch.
