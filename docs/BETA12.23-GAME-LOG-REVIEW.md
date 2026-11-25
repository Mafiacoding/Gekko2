# BETA12.23 – Crash/Tekken transport, UV and display review

16 October 2026. Branch `codex/beta12-23-game-log-fixes`, based on
`8553e2d4177719be296aef76bd8a4d4d5fa8bb45` (BETA12.22).
Release `R1389-SPEED-BETA12.23-VU-VIF-UV`.

## Evidence from the owner's BETA12.22 Wii sessions

Session 2 is Crash E3, identified by its legacy FILEIO/font/resource requests.
Session 1 is consistent with Tekken, rather than identified merely by the
shared TEKKEN_DMA diagnostic label (that label appears for every game).
Neither supplied session reports a GX fence timeout. Both experimental
options were active; these logs cannot isolate their individual effects.

Crash eventually stops submitting new GS draws while its EE continues.
The final snapshot has more than 3.46 billion producer retries at a second
SIGNAL, PATH1 ownership, a pending XGKICK and unchanged output fingerprints.
This establishes a transport stall, not its full upstream cause. The parsed
packet length is suspicious but within the maximum PACKED-tag encoding;
it is not by itself proof of memory corruption. Discarding SIGNAL, replacing
EOP or resetting the GIF to force progress would hide the failure.

Tekken has zero SIGNAL stalls. Across 437 measurement intervals its median
is 0.199 guest VBlanks/s, weighted mean about 0.231, maximum 0.799. These
are emulated VBlank rates, not distinct rendered game frames. Crash's owner
reports near 2 FPS at times; the prolonged stalled part makes a whole-session
average unsuitable for judging active rendering speed. Its final CPU samples
are almost entirely EE-side work/retry activity, not GX waiting.

The raw owner logs remain private and are not included in this branch,
source archive, downloads or PR. Only aggregate findings are recorded here.

## Implemented corrections

1. **VU ILW/ISW/ILWR/ISWR lane masks.** Stores previously wrote only the
   first selected component. Loads selected the first component, even for
   a zero mask, whereas x/y/z/w priority leaves the last selected component.
   Both scalar semantics and emitted PPC now honor the mask. Base addresses
   are captured before aliased destination writes. Loads preserve VI00 and
   stores zero-extend VI16. Synthetic tests failed on the previous source.
2. **VIF microprogram interlocks.** VU execution may yield on GIF
   backpressure or its step cap. FLUSHE/FLUSH/FLUSHA, MSCAL/MSCALF/MSCNT
   and MPG must not blindly proceed through that running program. Held
   command/payload words remain in the bounded FIFO and replay exactly once
   after the relevant VU/GIF condition clears. FLUSHE does not incorrectly
   wait for all GIF paths. UNPACK is not globally blocked: legitimate
   double-buffer work remains possible. A held barrier is checked before
   copying/reparsing its FIFO on every service tick.
3. **Fractional UV.** Natural and PACKED UV decode now preserve the four
   fractional bits and mask reserved bits. Exact 10.4 values survive sprite,
   independent triangle, strip/fan assembly and GX-source shading. Previously
   truncating to integer texels changed bilinear phase and texture placement.
   Shared GIF state uses doubles for these texel coordinates; raw cross-version
   structure snapshots are not a compatibility contract. STQ is unchanged.
4. **4:3 presentation on widescreen Wii.** A Wii configured as 16:9 now
   pillarboxes the standard 4:3 PS2 picture to 75% of XFB width. A 4:3 Wii
   keeps identical presentation. YCbCr pairs remain aligned; black borders
   are added only after coherent GX/software scanout, never inside GS VRAM.
   This assumes the guest uses its normal 4:3 mode; automatic recognition
   of games' internal widescreen modes is not implemented.
5. **Diagnostics.** GIF_BLOCKED_STATE adds CSR/IMR/INTC and parser position
   to distinguish an unacknowledged real SIGNAL from malformed producer data.
   GS_PRESENT_LAYOUT records SMODE1/CMOD, display offsets/magnification and
   the Wii aspect setting. Logging remains at existing snapshot frequency.

These are reproduced emulator defects, not confirmed explanations of all
observed Crash/Tekken graphics or gameplay failures. The supplied logs do not
contain the actual VU words needed to prove which lane-mask form caused a
particular packet. The follow-up build must be tested on Wii.

## PCSX2 and Play! comparison

Behavioral references, not a claim of transplanted game compatibility:

- Bundled PCSX2 `docs/reference/pcsx2/pcsx2/VUops.cpp`, `_vuILW`, `_vuISW`,
  `_vuILWR`, `_vuISWR`; `Vif_Codes.cpp` FLUSH/MSCAL/MPG handlers.
- Bundled PCSX2 `GS/GSState.cpp`, `GSPCRTCRegs::SetDisplayRect`: frame-mode
  interlace halves framebuffer read height while retaining display geometry.
- [Play!](https://github.com/jpd002/Play-/tree/d44bfa1814b729d4a3ed3c6c3a945432d5ee5bbe),
  `Source/ee/Vif.cpp`, `Vif1.cpp`, `IPU.cpp`, `Source/gs/GSHandler.cpp`:
  busy-VU/MPG interlocks, separate display rectangles and circuit layers,
  resumable command decoding. No Play! code was incorporated.

| Area | Gekko2 coverage and remaining difference |
| --- | --- |
| Display geometry | Tekken's 640x224 field agrees with INT+FFMD in both references; it is not evidence that the internal resolution should be doubled. Physical 16:9 stretch is corrected here. Full CRT aperture/offset handling, PMODE two-circuit merge and temporal deinterlacing remain incomplete. |
| GS sampling | All 13 format inputs exist, with software fallback. Physical cross-format CLUT aliases, full per-pixel LOD, mixed min/mag filtering, MTBA and trilinear filtering remain incomplete. |
| GS/GX rendering | Precise software destination tests and bounded hybrid paths exist. Full native TEV equivalents, framebuffer feedback/shuffle, AA1 and broad varying-depth/Gouraud paths remain incomplete. The software fallback is still expensive. |
| VU/GIF | Mask/interlock defects above are corrected. Exact VI branch dependency latency, broad VU floating-point bit accuracy and asynchronous arbitration/timing need further verification. Unknown-opcode counts alone do not identify missing legal instructions. |
| DMA | Normal/chain GIF/VIF, MFIFO and selected reverse transfers exist. Reverse-chain VIF1 and interleave outside supported SPR paths still need implementation/verification. |
| IPU | Resumable IDEC/BDEC, coefficient/IDCT, CSC/PACK and DMA tests exist. Complete movie-stream and FIFO/timing coverage is not certified. Crash's supplied session issued no IDEC/BDEC/CSC/PACK, while Tekken did use BDEC/VDEC/FDEC; an IPU rewrite is not a demonstrated Crash fix. |
| EE/IOP JIT | Audited clean 128-bit chains remain opt-in; general dirty cross-block residency and complete native opcode coverage remain open. Game-specific PCSX2 compatibility settings cannot simply be enabled in a different core. |

A complete PCSX2/Play! game-compatibility audit requires executable workloads
and image/state comparisons, not just opcode inventories. No BIOS/disc was
available in this environment. No claim of reaching in-game is supported yet.

## Tests and speed measurement

`tools/test_beta12_23_host.py` runs 47 host suites, including the inherited
37 BETA12.22 gates plus VU/VIF/UV/display tests. New independent cases cover
128 scalar VI-mask combinations, 11 VIF interlock scenarios, 32 fractional UV
phases/encodings with triangle/sprite/GX-source pixels and 256 XFB fits.
The old VIF opcode test now supplies real terminating E/delay microprograms:
an all-zero infinite microprogram must legitimately block the next MSCNT.
The generic host driver now recognizes relative white-box source includes.

`tools/verify_vu_integer_masks.py` checks 512 independent emitted-PPC cases
with all masks, aliases, both memory sizes and nonvolatile-register ABI.
Relative to the already-corrected unoptimized implementation, retaining only
the observable final local-memory load and hoisting invariant store operands
reduces total executed PPC instructions **8,448 → 6,272 (25.8%)** in this
synthetic set. This is not a whole-core benchmark or measured Wii FPS gain.
The comparison baseline is the corrected implementation before that local
optimization, not the incorrect BETA12.22 semantics.

Actual linked PPC checks exercise seven VIF barriers, sixteen fractional UV
triangle pixels and 1,920 aspect-fit YCbCr pairs. Inherited GS format,
XGKICK, IPU, DMA, VIF and GX-fence fixtures also run against the new ELF;
platform/GPU services remain modeled. Their output is included with the
build artifacts. All 20 chain/PSM mode comparisons pass. Synthetic AOT fixtures generated outside tracked source pass 1,024 VU0/VU1 pipeline comparisons.

## Builds and next Wii verification

Pinned SDK: devkitPPC r50 / GCC16.1.0 / libogc3.1.0,
`devkitpro/devkitppc@sha256:4c919aa26151dd43d88ca28c922d1fe2409579a8ba60ef56517baf1abdfb1a48`.
`tools/build_beta12_23_games.sh` retains the SPEED/DSI/AOT/GX/ARM/idle/Lightrec
flags and separate chain/control directories. Both DOL/ELF checks pass.
Static MEM1 remaining: chain-capable 2,702,188 bytes; control 2,838,924 bytes.
Runtime heap/stack headroom and physical Wii boot are not established by this.

DOL SHA256:

- Chain-capable: `57bd8121a19cf23b6ec4a208e0577c46114e372002d9dfe59d8a947c1ea44a3a`
- Compile-time chain excluded: `dc76be368cc833e0aeb3e1e26f784f54cf9e8071b8f803c07efba44f98baed28`

Both experimental options remain factory OFF. Start a new Cold Boot with
both OFF for BIOS and each game, then compare chain alone, PSM alone and both.
Retain BETA12.22 for rollback. For Tekken, match Wii's configured TV aspect to
the TV's actual mode; compare the character menu proportions. Preserve the
new SESSION/GIF_BLOCKED_STATE/GS_PRESENT_LAYOUT snapshots. Crash must pass
its previous stalled stage before claiming that the SIGNAL deadlock is fixed.
No physical-Wii BETA12.23 gameplay, speedup or full graphics fix is claimed.
