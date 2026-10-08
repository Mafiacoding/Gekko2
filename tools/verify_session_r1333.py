"""Linked PPC cache-budget replay, controller intent and boot-local resets.
Platform allocation/libc are mocked. Costs count PPC instructions, not Wii FPS.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
def profile():
 dest=0x81760000;call('ee_jit_get_cache_profile',dest)
 return dict(zip(['lookups','hits','misses','collisions','stale','attempts','installed','failures','compile_tb','compile_samples'],struct.unpack('>10Q',u.mem_read(dest,80))))
results=[]
for reuse in [0,8192]:
 word(mask_addr,initial&~8192&~(1<<19)|reuse);setup()
 total[0]=0;active[0]=True
 for budget in [8,2]*16:
  word(state+off['pc'],base);word(state+off['next_pc'],base+4)
  assert call('ee_core_step_n',budget)==budget
 active[0]=False;assert reg2()==160 and executed()==160
 result=profile();result['PPC_instructions']=total[0];results.append(result)
assert results[0]['installed']==3 and results[1]['installed']==2,results
assert results[1]['hits']==30 and results[1]['PPC_instructions']<results[0]['PPC_instructions'],results
assert call('ee_jit_get_budget_cache_stat',2)==0 and u.reg_read(UC_PPC_REG_4)==1
print('EE_BUDGET_VARIANT_COST',json.dumps(results))
# A source mutation changes both budget variants correctly. Per-word prepares
# remain authoritative even if a write notification was missed by a caller.
for budget in [8,2]:
 word(state+off['pc'],base);word(state+off['next_pc'],base+4)
 before=reg2();u.mem_write(ram+base,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|9))
 call('ee_jit_notify_physical_write',base,4)
 assert call('ee_core_step_n',budget)==budget and reg2()==before+budget+8
 u.mem_write(ram+base,struct.pack('<I',(9<<26)|(2<<21)|(2<<16)|1))
 call('ee_jit_notify_physical_write',base,4)
call('ee_jit_reset_stats_for_test');assert profile()['installed']==0
assert call('ee_jit_get_budget_cache_stat',1)==0 and u.reg_read(UC_PPC_REG_4)==0
word(mask_addr,initial)
# Actual compiled controller helper, including held-button edge semantics.
START=0x1000;Z=0x10;A=0x100;B=0x200;dest=0x81761000
for held,down,expected,guest in [
 (START,START,1,0),(START,0,0,0),(A,A,0,A),
 (START|Z,START,2,0),(START|Z,Z,2,0),(START|Z,0,0,0),
 (Z|A,A,0,START),(Z|A,0,0,START),(Z|A|START,START,2,START),
 (B,B,0,B),(Z,Z,0,Z)]:
 assert call('wii_session_controls',held,down,dest)==expected
 assert int.from_bytes(u.mem_read(dest,2),'big')==guest
# GIF render counters are genuinely boot-local, not inherited from prior runs.
for name in ['g_draw_routes','g_gst','g_gfb','g_gstate_overflow','g_sprite_cached_draws']:
 u.mem_write(syms[name],b'\xff'*8)
call('gif_init')
assert all(call('gif_get_render_work',n)==0 and u.reg_read(UC_PPC_REG_4)==0 for n in [0,10,11])
assert call('gif_get_gouraud_stat',0)==0 and u.reg_read(UC_PPC_REG_4)==0
call('ppc_dynarec_reset_translation_stats')
for n in ['ppc_dynarec_get_allocated_bodies','ppc_dynarec_get_word_spills','ppc_dynarec_get_resident_blocks']:
 assert call(n)==0 and u.reg_read(UC_PPC_REG_4)==0
print('PASS budget variants/source mutation, START pause intent, guest START/HUD chords and boot counter resets')
