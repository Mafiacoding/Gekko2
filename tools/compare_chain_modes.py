#!/usr/bin/env python3
"""Reproducible warm-cache OFF/ON work count; not a Wii timing benchmark."""
from pathlib import Path
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))
results=[];reference=None
for mode in (0,1):
 result,successors=run(True,128,0,0,chain_mode=mode,measure=True)
 if reference is None:reference=result
 else:assert result==reference
 results.append(dict(chain_enabled=mode,guest_retired=128,executed_ppc_instructions=total[0],
                     native_successors=successors,psm_enabled=0))
print(json.dumps(dict(scope='synthetic linked PPC; modeled platform; no Wii timing or FPS claim',results=results),indent=2))
