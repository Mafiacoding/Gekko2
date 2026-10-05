#!/usr/bin/env python3
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'source/core/recompiler/vu_jit.c'
s=p.read_text()
old='#include "core/hw/vu_opcodes.h"'
new='#include "hw/vu_opcodes.h"'
if s.count(old)!=1:
    raise SystemExit(f'expected one generated include anchor, found {s.count(old)}')
p.write_text(s.replace(old,new,1))
print('R1326 generated VU opcode include path fixed')
