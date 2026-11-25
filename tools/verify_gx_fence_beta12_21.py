#!/usr/bin/env python3
"""Execute linked PPC fence decisions with modeled PE tokens and clock.
No real GPU/IRQ/FPS evidence; verify stale callbacks/idle cannot fake success.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),
             'ee_ppc_fixture_internal.py','exec'))
u.mem_map(0xcc001000,4096);u.mem_map(0xcc003000,4096)
names=['GX_SetDrawDoneCallback','GX_SetDrawDone','GX_GetDrawSync','GX_SetDrawSync',
       'GX_GetGPStatus','GX_AbortFrame','gx_wait_timeout_marker']
hooks={syms[n]:n for n in names}
scenario={}
def finish(value=0):
    u.reg_write(UC_PPC_REG_3,value);u.reg_write(UC_PPC_REG_PC,u.reg_read(UC_PPC_REG_LR))
def hook(uc,address,size,user):
    name=hooks.get(address)
    if name:
        if name=='GX_GetDrawSync':
            if scenario['token'] is None:finish(scenario['old'])
            else:
                scenario['reads']+=1
                finish(scenario['token'] if scenario['complete'] and scenario['reads']>=3 else scenario['old'])
        elif name=='GX_SetDrawSync':
            scenario['token']=uc.reg_read(UC_PPC_REG_3)&65535
            assert scenario['token']!=scenario['old'];finish()
        elif name=='GX_SetDrawDone':
            if scenario['callback']:word(syms['gx_done_flag'],1)
            finish()
        elif name=='GX_GetGPStatus':
            for r,v in zip(range(3,8),(0,1,1,1,0)):
                uc.mem_write(uc.reg_read(UC_PPC_REG_0+r),bytes([v]))
            finish()
        elif name=='GX_AbortFrame':scenario['aborts']+=1;finish()
        else:finish()
        return
    instruction=int.from_bytes(uc.mem_read(address,4),'big')
    if instruction&~(31<<21)==0x7c0c42e6: # mftb rN
        scenario['clock']+=1000000
        uc.reg_write(UC_PPC_REG_0+((instruction>>21)&31),scenario['clock'])
        uc.reg_write(UC_PPC_REG_PC,address+4)
u.hook_add(UC_HOOK_CODE,hook)
for complete,callback,old in [(1,0,7),(1,1,7),(1,0,65535),(0,1,7),(0,0,65535)]:
    scenario.update(complete=complete,callback=callback,old=old,token=None,reads=0,clock=0,aborts=0)
    word(syms['gx_wait_failed'],0);word(syms['render_enabled'],1)
    assert call('gx_cpu_wait',1)==complete
    assert int.from_bytes(u.mem_read(syms['gx_wait_failed'],4),'big')==1-complete
    assert scenario['aborts']==1-complete
    if complete:assert scenario['reads']>=3
    else:
        assert int.from_bytes(u.mem_read(syms['render_enabled'],4),'big')==0
        old_reads=scenario['reads'];assert call('gx_cpu_wait',1)==0
        assert scenario['reads']==old_reads # terminal fault cannot issue/reuse another fence
print('PASS linked PPC GX fence: token completion without IRQ, stale callback/idle rejection, 16-bit wrap, abort failure and sticky fault')
