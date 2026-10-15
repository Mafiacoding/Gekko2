# R1318 working notes — precise-cache collision and eviction safety

R1318 hardens the 256-entry direct-mapped precise EE block cache. Every successful executable allocation install receives a non-zero serial identity. A colliding block is published only after translation and finalization have succeeded; allocation/translation/finalization failure therefore leaves the old resident block untouched. `precise_active` forbids replacement or release while a native chain may still hold a code pointer.

The warm successor resolver now snapshots and rechecks allocation identity around its live source lookup in addition to R1316/R1317 source-page and mapping generations. This does not add patched tail links: chaining remains the existing call-based guarded mechanism. Collision replacements are counted for diagnostics.

Host verification covers a real direct-map PC collision, replacement serial change, blocked replacement while active, serial wrap skipping zero, active reset pinning and inactive reset/release. Structural source/mapping/MMI audits and the full host regression suite remain mandatory before the source commit is pushed. These tests are not Wii hardware performance evidence.
