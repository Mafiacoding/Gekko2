# R1267: bounded EE JIT ownership and cached rejection

## Changes and reason

The PC-tagged JIT frontend previously remembered only successfully compiled
functions. A broad COP2/MMI prefilter also admits encodings the translator
does not implement. Repeated execution of such an encoding allocated a code
buffer, attempted compilation, freed it and interpreted it on every visit.
R1267 caches deterministic rejection using both execution PC and raw opcode.
A changed instruction word resolves again, so self-modifying/overlay code
can become supported immediately. Rejection returns zero and executes the
existing interpreter path; it never pretends to retire guest work itself.

Allocation and finalization failures remain transient and are retried.
The owning 8,192-entry cache now stops new compilation when full. Existing
compiled functions remain usable. An uncached encoding is interpreted without
allocating an unowned executable buffer. Insertion explicitly reports ownership
transfer; a refused insertion frees its buffer before returning. The previous
full-cache path let unowned buffers survive indefinitely, and reset could not
find them all. Single-threaded dispatch is unchanged.

L0 gains a rejection byte; alignment grows its PPC entry from 12 to 16 bytes,
adding 64 KiB for 16,384 entries. This is a bounded memory tradeoff. New compile
attempt and rejection-hit counters are appended to the existing performance
log as `JIT_CACHE compiled=... attempts=... rejected_hits=...` in
`sd:/pcsx2/R1267-boot.log`. They do not affect guest scheduling.

## Evidence

- 178/178 native regression executables pass. The new cache frontend test
  checks 1,030 assertions with a controlled native compiler backend, including
  word replacement, PC zero, negative reuse, allocation/finalization retry,
  cache-full behavior and reset. It deliberately cannot execute PPC on x86;
  the real 32-bit build retains all layout assertions.
- Both actual Wii ELFs retain 188 inherited shared-core PPC checks.
  The JIT ELF adds seven actual frontend groups plus one EE integration group
  (196 total); the Interpreter ELF adds one disabled-JIT guard plus the same
  integration group (190 total). Compilation and generated ADDIU execution
  are real PPC; allocation and cache-maintenance platform calls are controlled
  by the Unicorn harness. Test setup resets cache storage directly; actual
  reset behavior is covered by the native frontend test.
- The same QFSRV encoding called 1,000 times in the emitted R1266 JIT ELF
  causes 1,000 allocations and frees. R1267 causes one allocation/free and
  999 negative hits. This measures that isolated repeated-opcode case, not
  its frequency in BIOS or game workloads and not FPS.
- Real `ee_core_step_n` integration executes a rejected instruction twice,
  advances PC normally, preserves registers and does not halt. This checks
  that negative hits still reach the interpreter and normal epilogue.
- Native genuine BIOS continuation from R1266's Browser-Back checkpoint
  renders two further menu frames with PAD released and HALT=0, ending at
  EE 1,202,250,643 / PC 0x0022e0d8. Native builds execute the interpreter;
  this is a menu regression check, not a full BIOS run under the PPC JIT.

The R1266 UV filtering correction, Browser cycle, configuration persistence
and four new emitters remain included. The 9,400 generated-block comparisons
from R1266 remain relevant; the translator itself is unchanged this round.
Read R1266-HANDOFF.md and R1265-HANDOFF.md for their scoped runtime evidence.

## Reproduce

Build with tools/build_r1267.sh and the supplied devkitPPC/libogc setup.
Run tools/verify_regressions.py for the native suite. Run
`PYTHONPATH=<unicorn-path> python3 tools/verify_ppc_cache_r1267.py <ELF> --nm <powerpc-eabi-nm>`
for compiled target verification. The same driver accepts the R1266 JIT ELF
for the allocation baseline. For native BIOS frame capture, use
verify_osdsys_render_r1266.c and your own authentic checkpoint.

The archive contains source, Interpreter/JIT ELF and DOL, cumulative Claude
patch/diff, public screenshots, verification logs and a SHA-256 manifest.
The patch targets pcsx2-wii-full-debug(1).zip and includes earlier fixes.
Private BIOS images, disc data, runtime RAM checkpoints and toolchain binaries
are excluded. The recommended BIOS regression baseline is the Interpreter
build; use the JIT build for controlled performance comparisons.

## Remaining work

Full EE/IOP/VU JIT is still unfinished. This changes frontend overhead and
allocation ownership, not block length or opcode coverage. Timer/interrupt,
VBLANK and RPC cadence is unchanged. Larger EE blocks require explicit
instruction-boundary integration; IOP and VU micro compilation need their
own verified backends. Mixed texture filters, fractional UV and other GS
accuracy remain open. No Dolphin or physical Wii FPS was measured. Tekken
was not rerun in R1267; Namco-logo progress does not establish gameplay.
