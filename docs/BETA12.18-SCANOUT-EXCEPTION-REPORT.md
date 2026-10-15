# Gekko2 SPEED BETA12.18 – GS scanout and EE exception guard

## Starting point
BETA12.17, with the SPEED/DSI/GX flags preserved. The `crash.bin` examined here is a raw 2352-byte-sector Mode2 CD image. The file itself and any private BIOS are **not** bundled.

## Changes implemented and checked
1. The former Wii software scanout and first-image detection hard-coded `gs_mem_read_psmct32` for all `DISPFB.PSM` values. When PSM is 2 or 10, this reads wrong swizzled bytes and can leave the Homebrew display blank. New `gs_scanout_read_rgb`, `gs_blit_scaled_psm_to_xfb`, `gs_display_has_rgb_psm`, and `gs_scanout_probe_rgb_psm` honor CT32, CT24, CT16, CT16S. CT16 and CT16S use their own GS addressing and expand RGB555 to RGB888; unsupported PSMs do not blindly reinterpret VRAM. 32-bit rendering remains byte-for-byte equivalent in its optimized host scanout path. For PSM16 the Wii software fallback runs; the GX scanout path remains only for PSM 0/1.
2. EE native block-preparation entry points return immediately when the state already reports a helper-raised exception, TLB fault, invalid PC or branch state, BEFORE calling `ee_core_block_peek`. No speculative second instruction read can be issued at that boundary. This is a safety fix, not an assertion that all helper exceptions are now cycle accurate.
3. Added `GS_SCANOUT_FORMAT` logging for the actual guest-selected scanout format and path.

## Game-specific binary evidence
Previous Wii BETA12.7 log: first picture reached, around 2 guest FPS in short samples; last hot address 0x001FC970–0x001FC988 in the VSync poll, selected PSMCT32 (PSM=0), and black frontbuffer at the end. The emulator exited through Homebrew, not a logged CPU crash.

Direct ISO9660 inspection of this upload shows `/LEVELS/B/TITLES/TITLE.GSC;1` and `TITLE.NUS;1`, but not `TITLE.LGT;1`; previous FILEIO trace records a failed `cdrom0:\\LEVELS\\B\\TITLES\\TITLE.LGT;1` open. This absence might be normal for an incomplete E3 demo. It does **not** by itself establish why the game was black or a safe substitute file. No resources were invented, no disc modified, and no automatic front/back framebuffer swap was introduced.

## Verification
- Host `test_beta12_18_scanout.c`: CT16 and CT16S RGB555, CT32/24 retained, first-image probe, YUV XFB, unsupported PSM guard – passed.
- Original `test_gs_display_scaling.c`: unchanged existing field/display scaling cases – passed.
- Three JIT-preparation paths contain the exception check before `ee_core_block_peek` – source-order assertion passed, not a full runtime native PPC exception test.
- devkitPPC 16.1.0 / libogc 3.1.0 cross-build with BETA12.17 SPEED/DSI flags – passed.
- `verify_dol_r1352.py`: ELF/DOL content and layout – passed.
- `verify_wii_memory.py`: 2,854,540 bytes static MEM1 remainder – passed.
- No physical Wii testing of BETA12.18 yet, therefore no reliable FPS measurement or confirmed Crash gameplay fix.

## Still open
The actual Crash E3 PSMCT32 black transition, missing optional demo data, remaining native helper exception precision, persistent PPC register residency across *independent* JIT blocks with ABI/generation preservation, GS DATE/blend/framebuffer cases beyond the protected software fallback, VU1/GIF and EE/IOP timing. These are not claimed complete.
