#!/usr/bin/env python3
from pathlib import Path
p=Path('source/core/ee/ee_core.c')
s=p.read_text()
old='if(st->vu0_branch_delay||st->vu0_ebit_delay||st->vu0_pipeline.q_pending)return 0;'
new='if(st->vu0_branch_delay||st->vu0_ebit_delay||(st->vu0_pipeline.q_pending||st->vu0_pipeline.p_pending))return 0;'
if s.count(old)!=1:
    raise SystemExit(f'guard mismatch: expected 1 old gate, found {s.count(old)}')
p.write_text(s.replace(old,new,1))
