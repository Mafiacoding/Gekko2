# R1282 — VU block residency, EE control dispatch and scanout

## Current status
Early alpha. BIOS/OSDSYS stability and performance come first. Owner confirms R1280 Remote behavior and real Wii Sony splash; stable Wii OSDSYS and playable games remain unconfirmed. README, STATUS, ROADMAP and CLAUDE instructions are rewritten around current implementation and evidence; obsolete accumulated README/STATUS/ROADMAP content is archived under docs/history.

## Implemented runtime changes
- VU0/VU1 straight-line blocks keep the eight invocation arguments in nonvolatile PPC registers for the entire block. Upper VI/ACC references use the resident pointers directly. Lower short forms restore only their required argument registers. This removes nested pair/upper stack frames; saves/restores the full caller EABI once. Existing exact-word cache checks, budgets, E-bit/branch boundaries, alias guards and transactional translation remain.
- EE J/JAL, ordinary/likely integer branches and JR/JALR stay inline in the CPU interpreter dispatcher, avoiding a native one-op function call. Native translations remain available. Delay slots, annulled slots, full-width predicates, links, Count and device/interrupt retirement are preserved.
- Contiguous PSMCT32 row reads hoist page/block/column Y work and share current VRAM directly. Native-width scanout uses this helper before the existing exact RGB conversion and vertical filter. Other scaling ratios retain the old path. No persistent pixel cache is added.

## Experimental EE block primitive
ppc_dynarec_translate_ee_alu_block compiles 2..8 pure ALU instructions with up to twelve resident GPR words. It preserves PPC nonvolatile registers, upper guest GPR halves and atomic decline; rejects memory, control, COP0 and unsupported forms. This is a register-state transformation only: it does not perform CPU/device retirement and is not enabled in the CPU loop. It supplies a tested building block for future exact-boundary integration, not a completed EE block JIT or measured runtime speedup. Linking, allocation policy and safe interrupt/exception exits remain open.

## Measured limits
All counts below execute real compiled/emitted PPC on Unicorn; allocation/cache services are mocked. They are not Wii cycles, FPS or BIOS workload throughput.
- Eight-pair warm VU call: VI arithmetic 768->680 (-11.46%), accumulator arithmetic 784->656 (-16.33%), local load/store 1120->856 (-23.57%), I-immediate 1095->1007 (-8.04%). Includes the block dispatcher.
- Controlled interleaved ADDIU/BEQ/NOP with HBLNK, 4096 EE / 512 IOP retirements: R1281 JIT 2632789 -> R1282 2544064 (-3.37%); Interpreter 2550016 ->2537731 (-0.48%). JIT remains slightly more expensive in this particular synthetic case.
- Equal 640-pixel GS reads: scalar API 33280 -> span14751 PPC instructions (-55.68%). This measures row-read work, not total rendering or overall frame time.
The Wii GPU does not execute these guest programs. A GX presentation/rasterization backend is still open; no 15–20 FPS Tekken or 10 FPS BIOS claim is supported.

## Validation
- 183 native regression tests pass on the final runtime source.
- 9760 generated VU blocks agree with the sequential C interpreter, lengths2..8, preserving EABI; compile decline remains transactional.
- 1200 new EE ALU block register-byte oracle cases, all21 forms, zero/alias/sign behavior, full128-bit GPR bytes and EABI preservation pass.
- Both final Wii ELFs pass inherited PPC suites and randomized profile/FPS tests, 1024 COP0 cases and44 timer/wrap cases each.
- Both builds pass350 GS span byte-oracle cases plus conversion/filter oracles for upsampling, identity and downsampling. Both pass176 short-branch retirement samples each.
- Native saved OSDSYS continuation advances about3.2M EE instructions without EE/IOP halt. This is warm-state continuity, not fresh cold boot or hardware proof.

## Installation
Rename PCSX2-Wii-R1282-Menu-JIT.dol to sd:/apps/pcsx2-wii/boot.dol. Preserve your BIOS/config/discs. Interpreter build is included for comparison. Next hardware evidence: sd:/pcsx2/R1282-boot.log, especially CPU_SAMPLE and EE_TIMERS. Existing Remote/Nunchuk, FPS/HUD switches and HBC exit controls remain.

## Distribution / GitHub
Public source excludes BIOS/discs, guest RAM/checkpoints, personal runtime logs and compiler/SDK binaries. Private warm state stays in a separate artifact. Public source includes license notices. Cumulative Claude patch/diff target the originally uploaded project and are applied/byte-verified before packaging. GitHub target authorized by owner: https://github.com/Mafiacoding/PCSX2-Wii; preserve existing history, no force push.
