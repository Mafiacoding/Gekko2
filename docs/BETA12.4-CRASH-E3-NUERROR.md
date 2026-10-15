# BETA12.4: Crash E3 NuError log probe

## Evidence from owner's physical Wii software log

- Source: `Gekko2-R1389-SPEED-BETA12.3-FILEIO-0000000002-disc-software.log` (private, not committed).
- Final wall runtime: 2,755,769 ms (~45 min 56 sec); EE retired 8,609,120,126 instructions.
- Final EE PC `00103d74`, EPC `00103d70`; scalar hot samples concentrate on `00103d70/00103d74`.
- `FIRST_IMAGE=0`, GS `PMODE=0`, `DISPFB=0`, `DISPLAY=0`; one sprite draw in total.
- `ISO_CACHE requests=1993 hits=1845 fills=148 physical_bytes=5524544 failures=0`.
- GX enabled and disabled tests reach the same output-deactivated condition.

## Independent disassembly of private user-supplied E3 disc

The game ELF has a loaded segment at `0x00100000`; the fatal path at
`0x00103ca8` prepares the NuError header via `sprintf` into stack `sp+0x1000`,
the error text via `vsprintf` into `sp`, and then executes:

```
00103d44 lw     v1,-0x717c(gp)
00103d48 beqz   v1,0x00103d70
00103d70 b      0x00103d70
00103d74 nop
```

The original image also has the `nu2.err` identity string at EE address
`0x00468800`, and a saved return PC in the NuError frame at `sp+0x1120`.
This is a deliberate *game* error stop, rather than evidence of a defective
Wii GX renderer. The **triggering error message** is not present in the
existing sampled logs; blaming FILEIO OPEN, a missing asset or VU timing
without that message would be speculation.

## Change

- Add `guest_error_probe.c` and header with full fingerprint guard: `pc`,
  unconditional self-loop machine instructions, game identity string, and
  bounded/valid guest stack before exposing any guest memory.
- Read guest memory directly, never through guest MMIO/TLB side-effect helpers.
- Capture two already-formatted strings: `heading` and `detail`, along with
  saved `caller` PC and selected `fileio_rpc_version`.
- Sanitize non-printable bytes and line delimiters; log once per boot inside
  the existing 5-second performance snapshot, no per-instruction polling.
- No guest memory/register mutations, no fabricated file successes, no
  bypassing the game's fatal branch.

Example expected log line (illustrative placeholder; actual text UNKNOWN):

```
CRASH_E3_NUERROR pc=00103d74 sp=xxxxxxxx caller=xxxxxxxx provider=xxxxxxxx heading="..." detail="..."
```

## Validation

- GCC host unit test `test_crash_e3_error_probe`: valid and rejected fingerprints,
  valid/corrupt stack, both delay-slot PCs, bounded strings, sanitized newlines.
- devkitPPC r50/libogc 3.1.0 Wii cross-build of native beta12.4 DOL: PASS.
- Wii memory and DOL/ELF structural verification: PASS.
- Wii/Dolphin boot: **NOT TESTED**.

After a fresh run, search the SD log for `CRASH_E3_NUERROR`. If the new
line appears, compare `detail` and `caller` to the FILEIO code; if missing,
we should instrument the fatal routine entry and relevant FILEIO traffic more
broadly rather than assume the stop is identical.
