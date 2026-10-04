"""Exact HBLNK counter gating and helper cost in real Wii ELF, not FPS."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
from unicorn import UC_HOOK_CODE
import json
active=[False];count=[0]
def hook(uc,address,size,data):
 if active[0]:count[0]+=1
u.hook_add(UC_HOOK_CODE,hook);u.ctl_remove_cache(0x80000000,0x81800000)
def put(addr,v):u.mem_write(addr,struct.pack('>I',v))
measure={}
for mode in [0,0x80,0x81,0x82,0x83]:
 call('ee_timers_init');call('ee_intc_init')
 call('ee_timers_mmio_write32',0x10000020,0xffff)
 call('ee_timers_mmio_write32',0x10000010,mode)
 u.mem_write(syms['g_bus_tick_counter'],struct.pack('>Q',100))
 count[0]=0;active[0]=True;call('ee_timers_tick');active[0]=False
 measure[hex(mode)]=count[0]
# Check high-word fallback, 32-bit carry and full-width overflow against Python.
cases=[1,18742,18743,18744,0xfffffffe,0xffffffff,0x100000000,
       ((1<<32)//18743+1)*18743-1,(1<<40)+1234,(1<<64)-2,(1<<64)-1]
for seed in cases:
 for mode,divisor in [(0x80,1),(0x81,16),(0x82,256),(0x83,18743)]:
  call('ee_timers_init');call('ee_intc_init')
  call('ee_timers_mmio_write32',0x10000020,0xffff)
  call('ee_timers_mmio_write32',0x10000010,mode)
  u.mem_write(syms['g_bus_tick_counter'],struct.pack('>Q',seed))
  call('ee_timers_tick')
  after=(seed+1)&0xffffffffffffffff
  assert bytes(u.mem_read(syms['g_bus_tick_counter'],8))==struct.pack('>Q',after)
  result=0x81745000
  assert call('ee_timers_mmio_read32',0x10000000,result)==1
  assert int.from_bytes(bytes(u.mem_read(result,4)),'big')==int(after%divisor==0),(hex(seed),hex(mode),after)
print('TIMER_MEASURE',json.dumps({'elf':Path(a.elf).name,'ppc_instructions':measure,'scope':'One timer-helper tick with three disabled counters, no Wii cycles/FPS.'}))
print('PASS actual Wii ELF 44 timer divider cases: 32-bit carry, 64-bit fallback and full-width wrap')
