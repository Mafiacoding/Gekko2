#!/usr/bin/env python3
"""R1312 EE JIT coverage audit.

Static reachability audit for the independently maintained EE front gate and
PPC translator. This catches the R922 class of regression where working PPC
code exists but GEKKO can never dispatch an opcode family to it.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
front = (ROOT / "source/core/recompiler/ee_jit.c").read_text(encoding="utf-8")
back = (ROOT / "source/core/recompiler/ppc_dynarec.c").read_text(encoding="utf-8")


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        raise SystemExit(f"audit: cannot locate {signature}")
    brace = text.find("{", start)
    if brace < 0:
        raise SystemExit(f"audit: malformed {signature}")
    depth = 0
    for pos in range(brace, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1:pos]
    raise SystemExit(f"audit: unterminated {signature}")


def uncomment(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


predicate = uncomment(function_body(front, "static int ee_jit_opcode_supported(uint32_t instr)"))
translator = uncomment(function_body(back, "int ppc_dynarec_translate_one("))
errors = []


def has_op(text: str, op: int) -> bool:
    return bool(re.search(rf"\bop\s*==\s*0x{op:02x}[uU]?\b", text, re.I))


for op, name in {0x00:"SPECIAL",0x01:"REGIMM",0x10:"COP0",0x11:"COP1",0x12:"COP2",0x1C:"MMI"}.items():
    if not has_op(predicate, op): errors.append(f"front gate missing {name} (op 0x{op:02x})")
    if not has_op(translator, op): errors.append(f"backend dispatch missing {name} (op 0x{op:02x})")

# R1281: instruction-keyed COP0 is deliberately limited to MFC0/MTC0.
cop0 = re.search(r"if\s*\(\s*op\s*==\s*0x10[uU]?\s*\)\s*\{([^{}]*)\}", predicate, re.I|re.S)
if not cop0:
    errors.append("cannot locate exact COP0 front gate")
elif "rs==0u||rs==4u" not in re.sub(r"\s+", "", cop0.group(1)):
    errors.append("COP0 front gate is no longer restricted to MFC0/MTC0")

# R1310 exact trap selectors must remain admitted.
for selector,label in [(8,"TGEI"),(9,"TGEIU"),(10,"TLTI"),(11,"TLTIU"),(12,"TEQI"),(14,"TNEI")]:
    if not re.search(rf"\btrap\s*==\s*{selector}[uU]?\b", predicate):
        errors.append(f"R1310 trap front gate missing {label}")
for funct,label in [(0x30,"TGE"),(0x31,"TGEU"),(0x32,"TLT"),(0x33,"TLTU"),(0x34,"TEQ"),(0x36,"TNE")]:
    if not re.search(rf"\bcase\s+0x{funct:02x}\s*:", predicate, re.I):
        errors.append(f"R1310 trap front gate missing {label}")

# R1311 signed-overflow forms must remain admitted.
for op,label in [(0x08,"ADDI"),(0x18,"DADDI")]:
    if not has_op(predicate, op): errors.append(f"R1311 overflow front gate missing {label}")
for funct,label in [(0x20,"ADD"),(0x22,"SUB"),(0x2C,"DADD"),(0x2E,"DSUB")]:
    if not re.search(rf"\bcase\s+0x{funct:02x}\s*:", predicate, re.I):
        errors.append(f"R1311 overflow front gate missing {label}")

# Reachability alone is insufficient if the native synchronous-exception
# machinery was removed by a refactor.
for token,label in [("EE_EXC_TRAP","trap"),("EE_EXC_OVERFLOW","overflow")]:
    if token not in translator:
        errors.append(f"backend missing native {label} exception marker {token}")

if errors:
    print("EE JIT coverage audit: FAIL", file=sys.stderr)
    for error in errors: print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("EE JIT coverage audit: PASS")
print("  SPECIAL/REGIMM/COP0/COP1/COP2/MMI have front + backend dispatch")
print("  COP0 one-op gate remains restricted to MFC0/MTC0")
print("  R1310 trap and R1311 signed-overflow gates remain reachable")
print("  native trap/overflow exception machinery remains present")
