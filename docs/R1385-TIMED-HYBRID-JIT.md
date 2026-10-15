# R1385: timed VU hybrid JIT

Production VU0 and VU1 now admit dynamically generated PPC blocks again.
R1384 disabled the older pipeline-blind block backend unconditionally.

The new backend emits bounded blocks of two to eight pairs. Each PPC call
contains exact upper/lower words and its expected PC. Before each pair the
shared scheduler checks those words against the owning micro memory and
checks branch/E/M state. A changed source returns before the stale pair.
Warm cache hits also validate every word, micro owner, mask and budget.

All pair execution uses the same FMAC/VF scoreboard, Q/P publication,
paired-source capture, branch/E retirement and XGKICK servicing as scalar
execution. The block runner never adds an extra cycle or retirement count.
Branches, E/M/D/T and lower pipeline operations end admission; scalar
execution handles the boundary and later dynamic blocks can resume.
Unsupported encodings retain the existing fallback and diagnostics.

This is a callback-based hybrid backend, not a fully inlined arithmetic
block compiler. Timed FMAC arithmetic uses exact-word native AOT when
available and the common C arithmetic otherwise. Other supported upper/
lower operations use the existing dynamic PPC emitters. Non-arithmetic
pairs can also use the existing fused native pair emitter after scheduler
preparation. FSSET, Q/P and source conflicts retain their scheduler path.

The caller context is transient, not checkpoint state. No serialized state
layout changes. JIT-off execution keeps the scalar runner. Generated code
owners are stable and a replaced block is released only outside execution.

Build with `tools/build_r1385.sh [private-profile.json]` and the recovered
Docker SDK devkitPPC r50 / GCC 16.1.0 / libogc 3.1.0. Public source retains
empty profile tables; private BIOS/micro/profile inputs stay external.

`tools/verify_vu_hybrid_r1385.py ELF --nm NM` executes linked production
PPC code, including emitted blocks and the VU0/VU1 runners. It checks delayed
flags and queries, RAW issue stalls, pending Q, bounded budgets, source changes
on warm hits and during a block, boundary refusal and callee-saved ABI.
Platform allocation/cache maintenance is modeled by the existing fixture;
this is not a physical Wii performance measurement.

Remaining limitations from R1384 still apply: host single-precision arithmetic
is not proven bit-exact for every PS2 mantissa/ACC/EFU case; scheduler slot
ratios, CLIP/VI forwarding, all GS/GIF/VIF interactions and physical Wii
runtime remain separate validation work. No full-game/FPS claim follows
from a successful build or PPC unit test.

## Executed validation

The final modern DSI build passes the linked timed hybrid suite, including
production VU0/VU1, the independent linked timed-FMAC suite, and 130 linked
EE/IOP/VU/BC2/jump/XGKICK oracles (34+32+48+8+6+2).
Seven host suites pass: hardware accuracy R1384, VU micro, VCALLMS R1356,
XGKICK timing AOT3, XGKICK boundary R1379, VU Q pipeline and checkpoint
configuration. The DOL matches ELF section bytes and contains the HID4
startup markers; MEM1 static end is 0x8163d954 with 1,844,908 bytes remaining.

`tools/game_survey_r1385.c` provides bounded real-BIOS cold-disc diagnostics
for owner-supplied images, including SYSTEM.CNF, CPU/video counters and
thread/semaphore states. It does not force guest progress. Host runs do not
execute PPC JIT code; the linked PPC suites validate that backend separately.
The supplied multi-volume archives were fully extracted with matching
archive entry sizes: Metal Slug 3 ISO 1,679,687,680 bytes and Klonoa 2 ISO
1,302,233,088 bytes. Private game data and BIOS are excluded from this package.

The 100-slice cold runs ended without CPU halt:

| Game | Serial | EE retired | IOP retired | PMODE | GIF QW | VU1 retired |
| --- | --- | ---: | ---: | --- | ---: | ---: |
| Metal Slug 3 | SLES_525.99 | 799,998,251 | 99,993,042 | 0 | 88 | 0 |
| Klonoa 2 | SLUS_201.51 | 782,262,258 | 99,993,042 | 0 | 88 | 0 |

These bounded runs observed no enabled display or VU1 workload. They do
not establish a permanent hang, game compatibility, gameplay or Wii FPS.
The next game investigation is the startup/thread/device path before
display enable; extending the JIT cannot itself prove that path fixed.
