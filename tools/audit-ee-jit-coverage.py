#!/usr/bin/env python3
"""R1312 EE JIT coverage audit.

Conservative source audit for the two independently maintained dispatch layers:
  * ee_jit_opcode_supported() in ee_jit.c
  * ppc_dynarec_translate_one() in ppc_dynarec.c

The script deliberately does not claim semantic correctness.  It catches the
class of regression fixed in R922: a backend implementation exists but the
GEKKO front-end gate can never send that opcode family to it.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FRONT = ROOT / "source/core/recompiler/ee_jit.c"
BACK = ROOT / "source/core/recompiler/ppc_dynarec.c"

front = FRONT.read_text(encoding="utf-8")
back = BACK.read_text(encoding="utf-8")

# Extract only the front-end support predicate, not historical comments later
# in the file.  Keep this bounded so an unrelated `op ==` cannot satisfy it.
m = re.search(
    r"static int ee_jit_opcode_supported\(uint32_t instr\)\s*\{(.*?)\n\}\n\n/\* Instruction-keyed",
    front,
    re.S,
)
if not m:
    raise SystemExit("audit: cannot locate ee_jit_opcode_supported()")
predicate = m.group(1)

# Families with substantial backend dispatch.  COP2/MMI intentionally use a
# blanket front gate because their backend has a second, exact sub-op decode.
# COP0/COP1 remain exact front gates and are audited separately below.
required_families = {
    0x00: "SPECIAL",
    0x01: "REGIMM",
    0x10: "COP0",
    0x11: "COP1",
    0x12: "COP2",
    0x1C: "MMI",
}

errors = []
for op, name in required_families.items():
    # Accept both `op==0x12u` and `op == 0x12u` spelling.
    pat = rf"\bop\s*==\s*0x{op:02X}[uU]?\b"
    if not re.search(pat, predicate, re.I):
        errors.append(f"front gate missing {name} (op 0x{op:02x})")
    if not re.search(pat, back, re.I):
        errors.append(f"backend dispatch missing {name} (op 0x{op:02x})")

# R1281 COP0 contract: only MFC0/MTC0 are safe in the instruction-keyed
# one-instruction cache.  ERET/TLB/EI/DI are control/system operations and must
# not accidentally become accepted by widening the front gate.
cop0 = re.search(r"if\s*\(\s*op\s*==\s*0x10[uU]?\s*\)\s*\{([^\n]*)", predicate, re.I)
if not cop0:
    errors.append("cannot locate exact COP0 front gate")
else:
    line = cop0.group(1).replace(" ", "")
    if "rs==0u||rs==4u" not in line:
        errors.append("COP0 front gate is no longer restricted to MFC0/MTC0")

# R1310/R1311 must remain reachable: REGIMM immediate traps and SPECIAL
# register traps are exact front-gate entries, not just backend code.
for token, label in [
    ("trap==8u", "TGEI"), ("trap==9u", "TGEIU"),
    ("trap==10u", "TLTI"), ("trap==11u", "TLTIU"),
    ("trap==12u", "TEQI"), ("trap==14u", "TNEI"),
    ("case 0x30", "TGE"), ("case 0x31", "TGEU"),
    ("case 0x32", "TLT"), ("case 0x33", "TLTU"),
    ("case 0x34", "TEQ"), ("case 0x36", "TNE"),
]:
    if token not in predicate:
        errors.append(f"R1310 trap front gate missing {label}")

# R1311 overflow instructions must still be admitted.  This does not prove
# exception semantics; the differential/oracle tests remain authoritative.
for token, label in [
    ("op == 0x08u", "ADDI"), ("op == 0x18u", "DADDI"),
    ("case 0x20", "ADD"), ("case 0x22", "SUB"),
    ("case 0x2C", "DADD"), ("case 0x2E", "DSUB"),
]:
    if token not in predicate:
        errors.append(f"R1311 overflow opcode front gate missing {label}")

if errors:
    print("EE JIT coverage audit: FAIL", file=sys.stderr)
    for e in errors:
        print(f"  - {e}", file=sys.stderr)
    raise SystemExit(1)

print("EE JIT coverage audit: PASS")
print("  SPECIAL/REGIMM/COP0/COP1/COP2/MMI have front + backend dispatch")
print("  COP0 one-instruction gate remains restricted to MFC0/MTC0")
print("  R1310 trap and R1311 signed-overflow front gates remain reachable")
