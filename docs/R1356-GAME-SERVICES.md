# R1356 Beta — game services and GX RGB scanout

R1355 remains the unchanged control build. This separate beta inherits its masked-interrupt deadline fix and all previous optimization switches.

## Added IOP service coverage

Registered sysclib exports now handle ten additional ordinals: memchr(10), memcmp(11), bcmp(15), strchr(21), strcmp(22), index(25), rindex(26), strlen(27), strncmp(29), strrchr(32). Existing memcpy/memmove/memset and SIO2 services are preserved, for a total of 30 supported registered services. Both IOP Service HLE and HLE RAM must be enabled for sysclib replacements; defaults and config format are unchanged.

Only actual registered RAM exports are intercepted, under the existing source-byte and pipeline guards. Read-only calls preserve caller registers other than return v0, zero register and the existing generation/return-PC updates. Guest pointer aliases are retained; comparison results use unsigned-byte subtraction. Bounds are checked before accessing actual allocated IOP RAM. Operations stop at 64 KiB; unterminated strings, out-of-bounds/MMIO pointers, larger requests, source changes, pending pipeline state and disabled options use the guest implementation. Zero-length comparisons do not read their arguments. No imports or guest instructions are rewritten. These are ABI-level HLE replacements with existing HLE timing, not instruction-exact or a new JIT backend.

IOP_SERVICE_READ logs calls, actual read bytes and declines. It resets at cold boot.

Reference: ps2dev/ps2sdk iop/system/sysclib/src/exports.tab and include/sysclib.h; musl/string comparison/search implementations, retrieved 2026-10-09. https://github.com/ps2dev/ps2sdk/tree/master/iop/system/sysclib

## GX black level

The CPU RGBA8 scanout packer previously clipped each RGB channel to 16..240. Resident surface presentation applied the same clamp in four extra TEV stages. R1356 removes both RGB clamps: GX textures/EFB retain full-range RGB and the display-copy engine handles XFB conversion. This fixes the known RGB-black lift and preserves gradients. GS VRAM, rasterization, alpha/depth and the existing software XFB conversion (black Y=16) are unchanged.

A separate issue remains if the framebuffer itself contains gray. A native Tekken cold run reads gray RGB values directly from GS VRAM, which cannot be corrected by the scanout-clamp fix. Do not threshold genuine gray pixels to black or claim that all Tekken gray backgrounds are fixed.

## Verification

455 linked PPC sysclib read-only ABI cases: all ten new services, unsigned-byte comparison, pointer aliases, zero lengths, boundaries, caps, source/option fallbacks, unchanged RAM and caller GPRs, 30-service registry replacement.
72 prior bulk ABI cases and six verified ARM transport/fallback cases pass. Existing retail sound-stream protocol tests pass.
Native and linked PPC full 256-level RGB upload tests pass; opaque scanout alpha and unchanged VRAM; software XFB black remains 0x10801080. DOL/ELF section bytes agree.

Tekken native survey executes a real supplied disc image and BIOS with actual controller input. Native host execution does not exercise the Wii PPC JIT, GX hardware or physical IOS. No full-game compatibility or speedup is claimed. See included verification output for the reached phase. No copyrighted BIOS, game image or checkpoint is included.

## Legacy PADMAN corrections

Legacy PAD RPC replies now use each command's actual result offset. Only padPortOpen binds a DMA destination; mode, lock and index fields are never DMA pointers. Port/slot/bounds/alignment are validated, an absent second pad remains disconnected during refresh, close/end release bindings, and unsupported actuators/pressure commands return failure. Mode information and digital/analog selection follow the legacy ps2sdk contracts.

The old DMA frame now advances with VBlank. Its copied padButtonStatus contains ok/mode/buttons at offsets 0/1/2, without the raw SIO2 0x5A marker. This fixes a one-byte shift of buttons and axes; digital/analog payload sizes are 4/8. Modern XPADMAN layout is unchanged. Reference: https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/pad/src/libpad.c and include/libpad.h.

Native tests pass for typed RPCs, all 16 button bits, absent ports, bounds, mode changes, frame refresh and checkpoint round trips. The older test_checkpoint_pad.c has seven hard-coded optional-tail-offset failures already present on R1355; its legacy-byte expectation also predates the corrected layout. The new focused test verifies real save/load, bindings, IRQ/clock preservation and input propagation without fixed tail assumptions. No checkpoint format changes were introduced.

## Tekken VU0 execution blocker and reached state

The supplied retail SLUS_200.01 reaches the title. The corrected-input native run then loads additional RSPU2 assets and halts at EE 3,518,879,050: opcode 0x4a0061b8 at PC 0x00343924 is VCALLMS. VCALLMS (function 0x38) and VCALLMSR (0x39) were missing from COP2 dispatch despite an existing VU0 micro executor. Both now enter that executor with the actual 15-bit immediate / CMSAR0 control-register index in microinstruction units, finishing prior VU0 work first. Reference: PCSX2 pcsx2/COP2.cpp and VU.h (REG_CMSAR0=27), retrieved 2026-10-09.

Sixteen native cases pass for both commands at eight nonzero/zero entry positions, E-bit delay slot, final TPC and completed running state. The native diagnostic restored the exact pre-fix halt checkpoint, rewound only the already-retired unsupported VCALLMS instruction and executed it normally; no credit, game state or guest code was patched. Execution reached EE 4,206,879,049 without another halt. This is a targeted checkpoint regression, not a fresh final-build cold boot or physical Wii test.

The selected display framebuffer still contains gray (0x008c8c8c); the renderer is writing a different framebuffer during the final observed phase. A menu/gameplay display or passage beyond Insert Coin has not been visually confirmed. No after-coin screenshot is claimed. Additional framebuffer/display synchronization investigation is still needed. GX's RGB lift, PAD layout and the observed VCALLMS halt are corrected; the complete intermittent Tekken gray issue remains unresolved.
