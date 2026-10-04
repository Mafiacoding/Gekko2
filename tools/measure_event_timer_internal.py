"""Actual linked PPC timer calls, original vs deferred; no Wii FPS claim."""
import argparse,sys,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
p=argparse.ArgumentParser();p.add_argument('before');p.add_argument('after');p.add_argument('--nm',required=True);a=p.parse_args()
loader=Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0]
results=[]
for elf in [a.before,a.after]:
 sys.argv=['loader',elf,'--nm',a.nm];g={};exec(compile(loader,'loader','exec'),g);u=g['u'];call=g['call'];total=[0];active=[False]
 def hook(uc,addr,size,data):
  if active[0]:total[0]+=1
 u.hook_add(UC_HOOK_CODE,hook)
 tests={}
 for mode in [0,1,2,3]:
  call('ee_timers_init');call('ee_timers_mmio_write32',0x10000010,0x80|mode);call('ee_timers_mmio_write32',0x10000020,65535)
  total[0]=0;active[0]=True
  for n in range(4096):call('ee_timers_tick')
  active[0]=False
  out=0x81770000;call('ee_timers_mmio_read32',0x10000000,out);value=int.from_bytes(bytes(u.mem_read(out,4)),'big')
  assert value==[4096,256,16,0][mode],(elf,mode,value)
  tests[str(mode)]={'PPC_instructions':total[0],'ticks':4096,'count':value}
 results.append({'elf':Path(elf).name,'timer_clock_sources':tests})
print(json.dumps({'results':results,'scope':'Actual PPC instructions including timer-call return; one timer, mock host; not Wii cycles or BIOS FPS.'},indent=2))
