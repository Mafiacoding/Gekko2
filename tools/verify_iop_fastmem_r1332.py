"""Linked PPC IOP direct RAM vs old helpers, plus CPU pipeline/MMIO oracle."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
body=Path(__file__).with_name('verify_iop_blocks_internal.py').read_text()
body=body[body.index('import random'):body.index('# Mixed instruction sequences')]
exec(compile(body,'IOP-layout-and-oracle','exec'))
# Boundary RAM is an input to the oracle, including loads before stores.
original_setup=setup
def setup(words,regs=None):
 original_setup(words,regs);u.mem_write(iram+0x1ffff0,bytes(16))
original_snapshot=snapshot
def snapshot():return original_snapshot()+bytes(u.mem_read(iram+0x1ffff0,16))
mask_addr=syms['gekko2_optimization_mask'];initial=int.from_bytes(u.mem_read(mask_addr,4),'big')
# Both option states: every byte/halfword/word width, sign, zero destination,
# base/destination alias, KSEG aliases, alignment and IsC.
digests=[]
for flag in [0,512]:
 sig=hashlib.sha256()
 word(mask_addr,initial&~512|flag)
 for op in [0x20,0x21,0x23,0x24,0x25,0x28,0x29,0x2b]:
  for addr in [0x180000,0x80180000,0xa0180000,0x80180001,0x801ffffc,0x80200000,0x1f801070]:
   for rt in [0,2,3]:
    regs=[0]*32;regs[2]=0x89abcdef;regs[3]=addr
    words=[(op<<26)|(3<<21)|(rt<<16)]+[(9<<26)|(4<<21)|(4<<16)|1]*7
    for isolate in [0,0x10000]:
     def extra():word(ist+io['cop0']+12*4,isolate)
     run_case(words,8,regs,extra)
 # Load delay, including same-register write on next instruction.
 regs=[0]*32;regs[3]=0x80180000;regs[2]=0x11
 run_case([(0x23<<26)|(3<<21)|(2<<16),(9<<26)|(2<<21)|(4<<16)|1,(9<<26)|(2<<16)|77]+[0]*5,8,regs)
 digests.append(sig.hexdigest())
assert digests[0]==digests[1],digests
print("IOP_FASTMEM_SIGNATURE",digests[0])
# Newly native COP0 transfers retain delayed MFC0 values and precise IRQs.
for rd in range(32):
 for rs in [0,4]:
  for rt in [0,2]:
   regs=[0]*32;regs[2]=0x10000
   words=[(0x10<<26)|(rs<<21)|(rt<<16)|(rd<<11),(9<<26)|(2<<21)|(4<<16)|1,(9<<26)|(2<<21)|(5<<16)|2]+[0]*5
   def extra():word(ist+io['cop0']+rd*4,0x12345678 if rd!=12 else 0x10000)
   run_case(words,8,regs,extra)
# COP0 data movement must actually execute in the native backend.
def route(n):
 hi=call("iop_core_route_stat",n);return (hi<<32)|u.reg_read(UC_PPC_REG_4)
for rs in [0,4]:
 setup([(0x10<<26)|(rs<<21)|(2<<16)|(12<<11)]+[0]*7)
 before=route(0);fallback=route(1);assert call("iop_core_step_n",1)==1
 assert route(0)==before+1 and route(1)==fallback
# A warm direct LW must avoid its C helper, while MMIO uses it.
word(mask_addr,initial|512);helper_calls=[0]
def helper_hook(uc,address,size,user):
 if address==syms['iop_mem_read32']:helper_calls[0]+=1
hook=u.hook_add(UC_HOOK_CODE,helper_hook)
cost=[]
for flag in [0,512]:
 word(mask_addr,initial&~512|flag);regs=[0]*32;regs[3]=0x80180000
 setup([(0x23<<26)|(3<<21)|(2<<16)]+[0]*7,regs)
 assert call('iop_core_step_n',1)==1
 word(ist+io['pc'],ibase);word(ist+io['next_pc'],ibase+4)
 before=helper_calls[0];total[0]=0;active[0]=True;assert call('iop_core_step_n',1)==1;active[0]=False
 # One read32 is the architectural instruction fetch; direct data LW adds none.
 calls=helper_calls[0]-before;cost.append({'fastmem':bool(flag),'PPC_instructions':total[0],'read32_calls_including_fetch':calls})
assert cost[1]['read32_calls_including_fetch']==cost[0]['read32_calls_including_fetch']-1,cost
assert cost[1]['PPC_instructions']<cost[0]['PPC_instructions'],cost
u.hook_del(hook);word(mask_addr,initial)
print('IOP_FASTMEM_COST',json.dumps(cost),'modeled PPC instructions, not Wii time')
print('PASS',cases,'IOP CPU/pipeline programs with Fastmem OFF/ON; real MMIO, IsC, bounds, aliases, alignment, zero/alias destination and no RAM data helper')
