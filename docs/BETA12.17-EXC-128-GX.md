# BETA12.17 - EE exceptions, 128-bit locality, GX Z-format guard

- EE native prepare callbacks do not clear an exception taken by a prior helper. The DSI LW/LWU fallback checks `exc_raised_this_step` and `mem_tlb_miss` before overwriting the guest destination.
- EE register-residency planner groups all four 32-bit words of a 128-bit GPR if multiple lanes are hot. Existing generation/refresh and canonical write-through remain in force. **This is locality within one native block, not persistent PPC registers across arbitrary linked blocks.**
- GX CPU-import depth tests clamp incoming Z to PSMZ16/16S or PSMZ24 bit width before comparison, matching the established GS software behavior. DATE/FBA/FBMSK continue to use software fallback; not all hardware alpha cases are completed.
- Caveats: Cross-block 128-bit register ABI, all precise helper exceptions, full GS alpha/framebuffer emulation and real Wii gameplay remain open.

## Verifikation und Abgrenzung

- Wii ELF/DOL strukturell verifiziert; statischer MEM1-Rest 2.855.948 Bytes.
- Host: `test_beta12_17_residency.c` hält vier Lanes des 128-Bit-GPR 5 zusammen (MMI2 PEXEW, drei Instruktionen).
- Host: `test_beta12_17_depth.c` – 32 Vergleiche der Tiefensättigung (Z32/Z24/Z16/Z16S).
- Host: `test_beta12_17_framebuffer.c` – sieben PSMCT32-/PSMCT24-Framebufferfälle (FBA, FBMSK).
- Exception-Offsets beim Wii-Build statisch validiert. **Ein kompletter laufender JIT-Exception-Wii-Test ist noch ausstehend.**
- GX-Blend/DATE/FBA/FBMSK in allen Hardwarepfaden sowie 128-Bit-Register über unabhängige Block-ABIs bleiben offen. Keine FPS- oder Gameplay-Zusage.
