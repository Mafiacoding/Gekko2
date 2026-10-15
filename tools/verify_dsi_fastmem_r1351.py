"""Actual linked PPC HTAB construction, DSI glue and patched LW fallback.
Unicorn does not implement Broadway's hash walker. Model privileged SPR/MMU
operations and exceptions; execute the shipped integer handler, glue, emitter,
RAM loads, normal-context memory helper and allocator without replacements.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
assert 'g_gekko2_dsi_active' in syms,'requires R1351B ELF'
objdump=str(Path(a.nm).with_name('powerpc-eabi-objdump'))
for function in ['gekko2_dsi_handle','gekko2_dsi_entry','ppc_code_cache_live_bounds']:
 dis=subprocess.check_output([objdump,'-d',a.elf,'--disassemble='+function],text=True)
 import re
 for line in dis.splitlines():
  match=re.match(r'\s*[0-9a-f]+:\s+((?:[0-9a-f]{2} ){3}[0-9a-f]{2})\s+(.*)',line)
  if not match:continue
  instruction=int(match[1].replace(' ',''),16);opcode=instruction>>26
  assert opcode not in [48,49,50,51,52,53,54,55,59,63],('FP in exception path',line)
  if match[2].startswith('bl '):assert '<ppc_code_cache_live_bounds>' in line or '<gekko2_dsi_handle>' in line,('unexpected fault-time call',line)
  assert not match[2].startswith('bctrl'),('indirect fault-time call',line)
print('PASS linked integer-only DSI glue/handler/live-owner audit; no allocation, IPC, stdio or memory helper calls')
u.mem_map(ram+0x400000,0x1c00000)
SPR={25:0,19:0,18:0,1008:0};SR={};machine={'msr':0x2010,'faults':0,'returns':0,'panic':0,'broken':False}
saved=[];alias_pages=set();W=0x20000000;helper_calls=[0]
def get(address):return int.from_bytes(u.mem_read(address,4),'big')
def walk(ea):
 vsid=SR.get(ea>>28,0)&0xffffff
 hash=(vsid&0x7ffff)^((ea>>12)&0xffff)
 api=(ea>>22)&63;table=0x80000000|(SPR[25]&0xffff0000)
 for secondary in range(2):
  group=table+(hash&0xfff)*64
  for slot in range(8):
   a=get(group+slot*8);b=get(group+slot*8+4)
   if a==(0x80000000|(vsid<<7)|(secondary<<6)|api):return (b&0xfffff000)|(ea&4095),b
  hash=~hash
 return None
def host(pa):return pa|0x80000000
def snapshot():
 return ([u.reg_read(UC_PPC_REG_0+i) for i in range(32)],u.reg_read(UC_PPC_REG_CR),u.reg_read(UC_PPC_REG_LR),u.reg_read(UC_PPC_REG_CTR),u.reg_read(UC_PPC_REG_XER))
def fault(pc,ea,dsisr=0x40000000):
 saved.append(snapshot());machine['faults']+=1
 SPR[19]=ea;SPR[18]=dsisr;SPR[272]=u.reg_read(UC_PPC_REG_11);SPR[273]=u.reg_read(UC_PPC_REG_12)
 u.mem_write(0x800000d0,struct.pack('>3I',pc,machine['msr'],u.reg_read(UC_PPC_REG_CR)))
 machine['msr']&=~(0x8000|0x2000|2)
 u.reg_write(UC_PPC_REG_MSR,0);u.reg_write(UC_PPC_REG_PC,syms['gekko2_dsi_entry'])
def hardware(uc,address,size,data):
 w=get(address);op=w>>26;xo=(w>>1)&1023;r=(w>>21)&31
 if address==syms['ee_mem_read32']:helper_calls[0]+=1
 def skip():uc.reg_write(UC_PPC_REG_PC,address+4)
 if address==syms['PPCExcptDefaultHandler']:
  machine['panic']+=1;uc.reg_write(UC_PPC_REG_PC,0x817ff000);return
 if op==31:
  spr=((w>>16)&31)|(((w>>11)&31)<<5)
  if xo==339 and spr in SPR:uc.reg_write(UC_PPC_REG_0+r,SPR[spr]);skip();return
  if xo==467 and spr in (25,26,27,1008):SPR[spr]=uc.reg_read(UC_PPC_REG_0+r);skip();return
  if xo==83:uc.reg_write(UC_PPC_REG_0+r,machine['msr']);skip();return
  if xo==146:machine['msr']=uc.reg_read(UC_PPC_REG_0+r);skip();return
  if xo==659:
   ea=uc.reg_read(UC_PPC_REG_0+((w>>11)&31));uc.reg_write(UC_PPC_REG_0+r,SR.get(ea>>28,0));skip();return
  if xo==242:
   ea=uc.reg_read(UC_PPC_REG_0+((w>>11)&31));SR[ea>>28]=uc.reg_read(UC_PPC_REG_0+r);skip();return
  if xo==306:skip();return
  if xo==982:
   ea=uc.reg_read(UC_PPC_REG_0+((w>>11)&31));uc.ctl_remove_cache(ea,ea+4);skip();return
 if op==19 and xo==50:
  restored=snapshot();expected=saved.pop()
  assert restored==expected,('DSI glue corrupted interrupted registers',restored,expected)
  machine['returns']+=1;machine['msr']=SPR[27]
  uc.reg_write(UC_PPC_REG_MSR,SPR[27]&0x2000);uc.reg_write(UC_PPC_REG_PC,SPR[26]);return
 is_probe=address==syms['gekko2_dsi_probe_word']
 is_site=w==0x7ce02c2c and get(address-8)==0x47445349
 if not(is_probe or is_site):return
 ea=uc.reg_read(UC_PPC_REG_3 if is_probe else UC_PPC_REG_5)
 translation=walk(ea)
 if not translation or machine['broken']:
  fault(address,ea);return
 pa,pte=translation;assert pte&3==3,'window must be read-only'
 page=ea&~4095
 if page not in alias_pages:uc.mem_map(page,4096);alias_pages.add(page)
 uc.mem_write(page,bytes(uc.mem_read(host(pa&~4095),4096)))
u.hook_add(UC_HOOK_CODE,hardware)
mask=syms['gekko2_optimization_mask'];initial=get(mask);word(mask,initial|1|64|512)
call('dsi_fastmem_reserve');before=bytes(u.mem_read(ram,8192))+bytes(u.mem_read(ram+0x1fffffc,4))
call('dsi_fastmem_init',ram,0x2000000)
assert call('dsi_fastmem_status')==1
assert call('dsi_fastmem_enabled')==1
assert call('dsi_fastmem_stat',4)==4 and machine['returns']==1
assert before==bytes(u.mem_read(ram,8192))+bytes(u.mem_read(ram+0x1fffffc,4))
for offset in range(0,0x2000000,4096):
 found=walk(W+offset);assert found and found[0]==(ram&0x3fffffff)+offset
for offset in [0x2000000,0x10000000,0x12000000,0x1fc00000,0x1fffffff]:assert walk(W+offset) is None
print('PASS actual PPC initialization: 8192 read-only RAM pages, no MMIO/ROM mapping, 4 probes, exact restored RAM')
setup();word(state+off['ram_size'],0x2000000)
def lw(rt=2,rs=1,imm=0):return (35<<26)|(rs<<21)|(rt<<16)|(imm&65535)
def execute(iw,addr):
 u.mem_write(state+16,struct.pack('>QQ',addr,0))
 assert call('ee_jit_try_execute_one_at',state,base,iw)==1
 return int.from_bytes(u.mem_read(state+32,8),'big')
for i,v in enumerate([0,1,0x7fffffff,0x80000000,0xffffffff,0x12345678]*8):
 offset=0x300000+i*4;u.mem_write(ram+offset,struct.pack('<I',v))
 got=execute(lw(),(0x80000000 if i&1 else 0xa0000000)+offset)
 assert got==(v if v<0x80000000 else 0xffffffff00000000|v)
assert call('dsi_fastmem_stat',0)==0
print('PASS 48 emitted PPC KSEG0/KSEG1 LW RAM cases with signed results and no faults')
# New scalar owner: mapped MMIO goes through the real helper after rfi.
call('ee_jit_reset_stats_for_test');addr=0xb000f430
expected=call('ee_mem_read32',state,addr)
faults_before=machine['faults'];got=execute(lw(),addr)
assert got==(expected if expected<0x80000000 else 0xffffffff00000000|expected)
assert machine['faults']==faults_before+1 and call('dsi_fastmem_stat',1)==1
site=call('dsi_fastmem_stat',2);assert get(site)>>26==18 and get(site)==get(site-4)
assert execute(lw(),addr)==got and machine['faults']==faults_before+1
assert execute(lw(),0x80300004)==1 # patched owner also retains RAM semantics
print('PASS first MMIO LW faults, restores all interrupted registers, patches in-owner branch; second call avoids DSI; later RAM stays exact')
# rt=$0 must still perform MMIO reads; unaligned/KUSEG never enter the window.
before_calls=helper_calls[0];execute(lw(0),addr)
assert helper_calls[0]==before_calls+1 and bytes(u.mem_read(state,16))==bytes(16)
for address in [0x80300001,0x00300004]:
 before_fault=machine['faults'];before_calls=helper_calls[0]
 execute(lw(3),address)
 assert machine['faults']==before_fault and helper_calls[0]==before_calls+1
print('PASS discarded MMIO read remains observable; unaligned/KUSEG use original helper without DSI')
# Corrupt/unowned/stale descriptors never get interpreted as a recognized site.
assert call('gekko2_dsi_handle',syms['gekko2_dsi_probe_word'])==0xffffffff
word(site,0x7ce02c2c)
for corrupt_index in [-3,-2,-1,0]:
 old=get(site+corrupt_index*4);word(site+corrupt_index*4,0)
 assert call('gekko2_dsi_handle',site)==0xffffffff
 word(site+corrupt_index*4,old)
for dar,dsisr in [(0x80000000,0x40000000),(0x30000000,0x42000000),(0x30000000,0x08000000)]:
 SPR[19]=dar;SPR[18]=dsisr
 assert call('gekko2_dsi_handle',site)==0xffffffff
call('ee_jit_reset_stats_for_test');assert call('gekko2_dsi_handle',site)==0xffffffff
print('PASS foreign, malformed, already patched and released owner declines')
# Real CPU entry, full-block/prefix machinery and residency/allocation enabled.
# Compare complete guest state and timer snapshots with the guarded path.
def ins(op,rs=0,rt=0,imm=0):return op<<26|rs<<21|rt<<16|(imm&65535)
program=[ins(15,rt=1,imm=0xb001),ins(35,1,2,0xf430),ins(9,2,3,1),
 ins(35,1,0,0xf430),ins(15,rt=4,imm=0x8030),ins(35,4,5),ins(9,5,6,2),ins(35,4,7,4)]
results={}
for enabled in [0,1]:
 word(syms['g_gekko2_dsi_active'],enabled)
 word(mask,(initial|1|8|16|32|64|512|2048|8192|(1<<19)|(1<<20))&~(1<<24))
 setup();word(state+off['ram_size'],0x2000000)
 u.mem_write(ram+base,struct.pack('<8I',*program));u.mem_write(ram+0x300000,struct.pack('<2I',0x80000001,123))
 word(state+off['pc'],0x80200000);word(state+off['next_pc'],0x80200004)
 before_patches=call('dsi_fastmem_stat',1);before_blocks=call('dsi_fastmem_stat',6)
 assert call('ee_core_step_n',8)==8
 results[enabled]=bytes(u.mem_read(state,size));call('ee_timers_snapshot',0x817f0000)
 results[enabled]+=bytes(u.mem_read(0x817f0000,64))
 if enabled:
  assert call('dsi_fastmem_stat',6)>before_blocks,'CPU block did not emit DSI sites'
  assert call('dsi_fastmem_stat',7)>0,'CPU never used speculative boundary'
  assert call('dsi_fastmem_stat',1)>=before_patches+2,'MMIO block sites did not fault/patch'
assert results[0]==results[1],'guarded/DSI CPU guest or timer state differs'
print('PASS actual EE block execution: generated block sites, RAM/MMIO LW, rt0, dependent GPRs, residency, exact guest/timer state versus guarded path')
variants=[program,program[:1]+[ins(35,1,1,0xf430)]+program[2:],
 [ins(15,rt=1,imm=0xb001),ins(4,0,0,2),ins(35,1,2,0xf430)]+[0]*5,
 program[:5]+[ins(35,4,5,1)]+program[6:],
 program[:4]+[ins(15,rt=4,imm=0x0030)]+program[5:]]
checks=0
for kind,words in enumerate(variants):
 for grant in [2,3,4,8]:
  pair=[]
  for enabled in [0,1]:
   word(syms['g_gekko2_dsi_active'],enabled);setup();word(state+off['ram_size'],0x2000000)
   u.mem_write(ram+base,struct.pack('<8I',*words));u.mem_write(ram+0x300000,struct.pack('<2I',0x80000001,123))
   word(state+off['pc'],0x80200000);word(state+off['next_pc'],0x80200004)
   assert call('ee_core_step_n',grant)==grant
   data=bytes(u.mem_read(state,size));call('ee_timers_snapshot',0x817f0000)
   pair.append(data+bytes(u.mem_read(0x817f0000,64)))
  assert pair[0]==pair[1],('DSI differential',kind,grant)
  checks+=1
print('PASS',checks,'guarded/DSI complete-state cases: short grants, rs=rt alias, branch-delay MMIO, unaligned and missing guest TLB')
call('dsi_fastmem_shutdown');assert SPR[25]==0 and SR.get(2)==0 and SR.get(3)==0 and call('dsi_fastmem_enabled')==0
# Failed alias test is a bounded decline with previous hardware state restored.
call('dsi_fastmem_reserve');machine['broken']=True
call('dsi_fastmem_init',ram,0x2000000);assert call('dsi_fastmem_status')==0xfffffffb
assert SPR[25]==0 and call('dsi_fastmem_enabled')==0
call('dsi_fastmem_shutdown');machine['broken']=False
print('PASS cold shutdown and failed self-test restore SDR1/SR2/SR3 and disable DSI')
SPR[25]=0x10000;call('dsi_fastmem_reserve');call('dsi_fastmem_init',ram,0x2000000)
assert call('dsi_fastmem_status')==0xfffffffd and SPR[25]==0x10000
call('dsi_fastmem_shutdown');SPR[25]=0
call('dsi_fastmem_reserve');call('dsi_fastmem_init',ram+32,0x2000000)
assert call('dsi_fastmem_status')==0xfffffffe and not call('dsi_fastmem_enabled')
call('dsi_fastmem_shutdown');fail_alloc=True;call('dsi_fastmem_reserve')
assert call('dsi_fastmem_status')==0xffffffff
fail_alloc=False;word(mask,get(mask)&~64);call('dsi_fastmem_reserve')
assert call('dsi_fastmem_status')==0xfffffff9
print('PASS existing SDR1, unaligned RAM, allocation failure and disabled arena decline without installing MMU')
# Unrelated faults reach the original default panic contract, never skip.
fault(syms['ee_core_get_state'],0x80000000)
u.emu_start(syms['gekko2_dsi_entry'],0x817ff000,count=1000000)
assert machine['panic']==1 and u.reg_read(UC_PPC_REG_11)==3
print('PASS unrelated DSI chains to libogc default handler')
print('Hardware MMU/cache timing and Wii boot remain unmeasured; this is a linked-code/platform-model test.')
