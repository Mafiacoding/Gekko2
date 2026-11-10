# GEKKO2 hardware profiler (experimental)

Standalone Wii homebrew; emulator runtime is unchanged. Build with `make` in
this directory (devkitPPC/libogc), or use the Actions artifact.

IOS222 is reloaded before FAT mounts only if its TMD exists and is not a stub.
The console and CSV report original/active IOS, revision and reload result.
No IOS is installed or patched. An existing IOS222 is retained.

MLOAD detection is independent of optional `/dev/gekko2` GKA1 v1 detection.
`mload_fill` uses 4 KiB seek/write chunks; `mload_copy` uses 64 KiB seek/read
chunks, matching `source/hw/arm_ram.c`. Only aligned, nonoverlapping, allocated
MEM1/MEM2 spans of 64 KiB–1 MiB are accepted. Cache flush/invalidation surrounds
synchronous IOS operations. Nonnegative IOS status (including 1) is accepted
only when every destination byte matches. Destination poison and a varying
copy pattern reject no-op/stale data. No CPU fallback can manufacture a pass.
Timing includes IOS/cache work; final verification is outside timing.
The optional GKA1 fill uses its own capability handshake and poisoned buffer.

Results: `sd:/gekko2-profiler.csv`, USB fallback. CSV distinguishes unavailable,
skipped, unsupported, failed, completed (not data-verified), and passed.
`verified=1` means a full data comparison passed. `work_units` means bytes for
RAM tests, vertices for GX; stride tests retain legacy nominal span counts,
not actual bytes transferred. Addresses identify actual heap placement; two
heap allocations are not proof of separate MEM1/MEM2 banks. RAM tests transfer
1 MiB once per operation; no stable-throughput or parallelism claim is made.
GX completion has no pixel correctness oracle, hence verified=0.

Actions validates ELF/DOL section parity, Wii startup signature and static MEM1
limits, and packages the validation reports. These are build/format checks;
physical Wii and Dolphin execution remain unverified. Run the DOL as boot.dol
in a Homebrew Channel app directory and inspect the resulting CSV on SD/USB.
