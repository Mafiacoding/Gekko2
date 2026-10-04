# Gekko2 R1303 — precise PPC block bookkeeping

R1303 removes the running r15 retirement counter from the integrated conservative EE block emitter. Each failed live prepare check branches to a stub returning its known number of completed instructions; successful execution returns the block length. Zero comparisons use cmpwi directly. All preparation, source/TLB validation, guest retirement, Count/Compare, timers and IRQ boundaries remain unchanged.

The common successful path avoids counter increments and register save/restore; out-of-line exit stubs slightly increase generated code size. This is not cross-boundary register allocation, broader memory/control blocks, IOP block linking or completion of the dynarec. GX and R1302 branding are preserved. Current launcher/log/meta revision is R1303.

Validation: 200 native tests, Interpreter/JIT cross-builds and four actual linked-PPC jobs. All 35 source-mutation exits across lengths 2–8, seven complete exits, exact retirement/PC, all 18 nonvolatile registers, budgets, eight IRQ positions/EPC, SMC, mapping replacement, far callback fallback and complete ALU/COP1 state comparisons pass. 66 full-VRAM signatures match between engines.

Same-service warm eight-instruction measurements from R1302 to R1303: ADDIU 3195→3177 and mixed COP1 3181→3163 PPC instructions. The 18-instruction reduction is about 0.6% in these isolated workloads, not Wii cycles or BIOS/game FPS. No fresh physical Wii or BIOS cold-boot claim.

Install the JIT DOL as sd:/apps/gekko2/boot.dol. Keep existing sd:/pcsx2/ BIOS/game/configuration paths. Compare paired builds with matching settings. Next work remains own register allocation, guarded memory/control blocks and IOP blocks, guided by actual Wii profiles.
