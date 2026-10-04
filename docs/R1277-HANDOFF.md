# R1277: cheaper independent display clock

The user's observed drops to 0.06/0.08 FPS around the menu/SCEA are not yet reproduced on physical Wii or Dolphin. They do not prove that SCEA reads themselves are the bottleneck. A shared hot-path cost was found: EE VBlank and GS VSYNC checks each computed the independent 64-bit display clock modulo 4,921,488 every guest instruction or parked tick.

The clock now maintains a derived uint32_t phase in the existing per-tick update, resetting at the frame boundary or 64-bit clock wrap. Both display edge checks read the same phase. Clock loads derive it once from the saved tick count, and cold reset uses that load path. Checkpoint format and the independent 64-bit clock are unchanged. Guest Count, instruction retirement, EE/IOP interleaving, VBlank, GS CSR/IRQ and parked-clock behavior remain in place. No ticks or rendering events are skipped; displayed FPS is not artificially increased.

## Controlled actual Wii ELF measurements

Eight scalar operations with complete EE epilogue, median of three warm samples. Platform allocation/cache maintenance mocked; these are PPC instruction counts, not Wii cycles, a full BIOS boot benchmark or measured FPS. Both JIT and Interpreter builds show the reduction. Roughly 18-19% less work in these cases does not establish the 7.5x improvement needed for 0.08 -> 0.60 FPS.

| Workload | R1276 PPC instructions | R1277 PPC instructions | Reduction |
|---|---:|---:|---:|
| ADDIU | 5017 | 4049 | 19.29% |
| LUI | 5129 | 4161 | 18.87% |
| ORI | 5145 | 4177 | 18.81% |
| ANDI | 5145 | 4177 | 18.81% |
| XORI | 5145 | 4177 | 18.81% |
| ADDU | 5129 | 4161 | 18.87% |
| SLL | 5113 | 4145 | 18.93% |
| SRL | 5241 | 4273 | 18.47% |
| SRA | 5241 | 4273 | 18.47% |
| SLLV | 5249 | 4281 | 18.44% |
| SRLV | 5249 | 4281 | 18.44% |
| SRAV | 5249 | 4281 | 18.44% |
| AND | 5289 | 4321 | 18.30% |
| OR | 5289 | 4321 | 18.30% |
| XOR | 5289 | 4321 | 18.30% |
| NOR | 5289 | 4321 | 18.30% |
| SLT | 5353 | 4373 | 18.31% |
| SLTU | 5361 | 4381 | 18.28% |
| SUBU | 5241 | 4273 | 18.47% |
| DADDU | 5289 | 4321 | 18.30% |
| DSUBU | 5289 | 4321 | 18.30% |

## Validation

182/182 native regressions pass, including actual checkpoint load API, independent Count writes, parked VBlank/GS behavior, high 64-bit restored clock phase, 32-bit Count wrap, 64-bit oscillator wrap and cold reset. Existing tests that directly changed the private clock now use the clock-load API. Wii test fixtures seed both derived phase and clock because console linking discards unused checkpoint setters; this limitation is explicit.

Both final ELF builds pass inherited EE/IOP/cache/IRQ/GS/config/VU0/VU1/VIF/native-font suites. Additional actual PPC tests verify high-clock frame edges and UINT64 wrap with no repeated edge on the next parked tick. Native authentic menu checkpoint continuation advances EE 2,045,035,061 -> 2,048,231,103, neither core halted; this is warm interpreter continuation, not a fresh cold PPC boot.

## Hardware diagnosis next

Use sd:/pcsx2/R1277-boot.log through the SCEA/menu slowdown. PERF records separate core_ms and blit_ms and report EE_per_s, guest_vblank_mHz and presents_mHz. The frontend executes up to 50,000 IOP slices (400,000 EE instructions) between presentations in throughput mode, so a low presents rate alone is not a direct measure of guest frame generation. This round does not change that presentation budget or claim smooth hardware menu interaction.

R1276's SD ISO/BIN browser and antialiased native launcher are retained. Full EE block JIT and GS texture/pipeline correctness limitations remain open; no new Tekken test is claimed. Public package contains no BIOS, game image or guest RAM; private native recovery checkpoint is separate.

Build tools/build_r1277.sh with devkitPPC/libogc, -j1, separate engines. DOL/ELF and cumulative patch/diff included.
