#!/usr/bin/env python3
"""Actual linked reset gate with modeled sync/shutdown failure injection."""
from pathlib import Path
source=Path(__file__).with_name('verify_gx_fence_beta12_21.py').read_text()
exec(compile(source.split('for complete,callback,old')[0],'verify_gx_fence_beta12_21.py','exec'))
scenario.update(clock=0)
counts={};failure=[False]
extra={syms[n]:n for n in ('gs_mem_sync','gs_gx_shutdown','gs_mem_gpu_bind')}
def reset_hook(uc,address,size,user):
 name=extra.get(address)
 if not name:return
 counts[name]=counts.get(name,0)+1
 if name=='gs_gx_shutdown' and failure[0]:word(syms['gx_wait_failed'],1)
 finish(1)
u.hook_add(UC_HOOK_CODE,reset_hook)
# An existing failure must stop before sync, drain or owner release.
word(syms['gx_wait_failed'],1);word(syms['surface_enabled'],1)
assert call('gs_gx_faulted')==1
assert call('gs_gx_reset_boot_state')==0 and not counts
# Shutdown may create a fresh failure despite successful VRAM sync.
word(syms['gx_wait_failed'],0);assert call('gs_gx_faulted')==0;failure[0]=True
assert call('gs_gx_reset_boot_state')==0
assert counts=={'gs_mem_sync':1,'gs_gx_shutdown':1},counts
assert int.from_bytes(u.mem_read(syms['surface_enabled'],4),'big')==1
print('PASS linked GX cold-boot gate: existing fault and new drain failure preserve ownership and evidence')
