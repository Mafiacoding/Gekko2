# PCSX2-Wii R1271

R1271 adds native fused VU instruction pairs to R1270. The full EE/IOP/VU JIT is not complete. R1270 remains the saved fallback.

## Implementation

ppc_dynarec_translate_vu_pair concatenates upper/lower native bodies in one function. It does not call an interpreter or re-enter C between the two instructions. I-immediate pairs write VI[21] before executing upper. Simple VI/register lower forms retain only the VI pointer; memory/branch forms retain their complete argument set. All callee-saved registers and the stack are restored. Translation is transactional: an unsupported lower never executes a supported upper.

vu_jit_try_pair has a bounded 128-entry owning cache, exact upper/lower word tags, replacement/free on collisions, cached rejections, and transient allocation/finalization retry. I participates in the tag; E/D/T flags remain outside arithmetic compilation. The micro scheduler re-fetches both words each pair and retains retirement, E-bit countdown and branch-delay handling at each pair boundary. Existing single-instruction JIT/interpreter paths handle declined pairs. The performance log adds a pairs counter. The existing sequential upper-before-lower model is preserved; this does not establish real PS2 parallel-pipeline accuracy.

## Validation and measured scope

- 11,360 generated PPC fused-pair cases: all 95 upper and 33 lower encodings sampled, masks/aliases, raw MIN/MAX and conversion inputs, I-immediate, local memory, ordinary branches and ABI.
- 180 native regressions pass in suite_results.json.
- Actual Wii ELF JIT and Interpreter builds pass inherited EE/IOP/VU integration and new pair dispatch tests. JIT additionally passes cache collision/word replacement, hot reuse, transactional decline, I visibility, transient retry.
- Identical warm VU1 loop, 302 retired pairs, three identical samples each: R1270 63,799 PPC instructions; R1271 57,137, a 10.442% reduction, identical final VF/VI/ACC/local state. This is an instruction-count result, not Wii cycles/FPS, BIOS reaction latency or a guaranteed whole-emulator speedup. The first all-argument-saving draft measured 62,591; the final emitter reduces saves for simple pairs.
- Authentic SCPH-50004 native interpreter BIOS continuation: Cross -> Browser -> Circle -> OSDSYS menu, no guest RAM/PC/menu flags forced. EE 1,558,401,798 to 1,679,274,722, 120,872,924 additional instructions, halt=0. Screenshots inspected. This resumes the existing BIOS checkpoint; generated PPC BIOS execution is not established by this native test.

## Reproduce

Build tools/build_r1271.sh with supplied devkitPPC/libogc and DEVKITPRO/DEVKITPPC, serialized -j1. verify_jit_vu_pair_r1271.py requires Unicorn. verify_ppc_pair_r1271.py takes ELF and --nm. compare_ppc_vu_r1271.py takes before ELF, after ELF and --nm. The private BIOS/checkpoint/config are not distributed.

## Remaining work

True multi-pair VU blocks/linking/register allocation and full pipeline/flags/Q/P/EFU behavior remain open. True EE/IOP blocks require preserving per-instruction Count/timer/interrupt/SIF behavior and the eight-to-one interleave boundary. Declined EE/IOP/VU paths, exceptions/load-delay accuracy and PS2 floating-point/timing accuracy remain partial. No new Tekken frame or physical Wii FPS measurement is claimed.

The Claude patch/diff are identical cumulative unified patches against original pcsx2-wii-full-debug(1).zip. Packaging validates dry-run, exact application/byte comparison, ZIP CRC and per-entry SHA-256. BIOS/game/raw RAM checkpoints are excluded.
