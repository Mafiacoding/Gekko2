#!/usr/bin/env python3
"""R1327 final dynarec gap inventory.

This is the last host-side closure gate before publishing a Wii checkpoint.
It does not claim physical-Wii runtime validation.  It composes the exact
coverage/safety audits accumulated through R1326 and fails if any known
ordinary/native gap or cache/control safety contract regresses.

Deliberate scalar/hardware boundaries are allowed only where the R1326 closure
audit already proves the boundary is explicit (privileged COP0/system forms,
instruction-granular event retirement, and scheduler-visible VU side effects).
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

COMMANDS = [
    [sys.executable, "tools/audit-ee-jit-coverage.py"],
    [sys.executable, "tools/audit_mmi_coverage_internal.py"],
    [sys.executable, "tools/audit_mmi_block_admission.py"],
    [sys.executable, "tools/audit_control_block_admission.py"],
    [sys.executable, "tools/audit_vu0_macro_coverage.py"],
    [sys.executable, "tools/audit_vu_micro_native_gaps.py", "--require-ordinary-native"],
    [sys.executable, "tools/audit_vu_pipeline_block_gates.py"],
    [sys.executable, "tools/audit_vu_upper_native_coverage.py"],
    [sys.executable, "tools/audit_precise_cache_eviction_r1318.py"],
    [sys.executable, "tools/audit_ee_ram_writer_invalidation.py"],
    [sys.executable, "tools/audit_r1326_final_closure.py"],
]

# R1327 is deliberately a composition gate: each child audit targets a
# different failure class and remains independently maintainable.
for cmd in COMMANDS:
    print("R1327:", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=ROOT, check=True)

# Pin the final architectural classification so a future refactor cannot turn
# an intentional scalar boundary into an undocumented missing implementation.
closure = (ROOT / "tools/audit_r1326_final_closure.py").read_text(encoding="utf-8")
required = {
    "privileged COP0 scalar boundary": "privileged COP0 stays an explicit scalar boundary",
    "instruction-granular EE event retirement": "ee_core_block_commit(st,0)",
    "instruction-granular IOP event retirement": "iop_core_block_retire",
    "serial/generation guarded EE links": "source->link_serial==serial",
    "IOP full-prefix SMC validation": "live!=slot->words[n]",
}
missing = [name for name, token in required.items() if token not in closure]
if missing:
    raise SystemExit("R1327 classification markers missing: " + ", ".join(missing))

print("R1327 final dynarec gap inventory: PASS")
print("  known ordinary EE/MMI/control/VU coverage gates are green")
print("  cache/link/source invalidation gates are green")
print("  remaining scalar/event paths are explicit correctness boundaries")
print("  physical Wii runtime validation remains a separate acceptance gate")
