"""Independent linked-PPC IOP timer oracle, including every IRQ tick."""
from pathlib import Path
import struct,random
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0],'loader','exec'))
base=[0x1f801100,0x1f801110,0x1f801120,0x1f801480,0x1f801490,0x1f8014a0]
irq_bits=[4,5,6,14,15,16]
scratch=0x81760000
def read(addr):return int.from_bytes(bytes(u.mem_read(addr,4)),'big')
def snapshot():
 values=[]
 for b in base:
  for offset in [0,4,8]:
   assert call('iop_timers_mmio_read32',b+offset,scratch)==1
   values.append(read(scratch))
 return values
def scalar(timers):
 irq=0
 for i,t in enumerate(timers):
  count,mode,target=t
  if mode&0x8000:continue
  count=(count+1)&0xffffffff
  if not mode&0x800 and count>=target:
   if mode&0x10 and mode&0x400:
    irq|=1<<irq_bits[i]
    if not mode&0x40:mode&=~0x400
   if mode&8:count=(count-target)&0xffffffff if target else 0
   else:mode|=0x800
  cap=0xffff if i<3 else 0xffffffff
  if count>cap:
   if mode&0x20 and mode&0x400:
    irq|=1<<irq_bits[i]
    if not mode&0x40:mode&=~0x400
   mode|=0x1000;count=(count-(cap+1))&0xffffffff;mode&=~0x800
  timers[i]=[count,mode,target]
 return irq
rng=random.Random(1309);cases=0
for scenario in range(48):
 call('iop_timers_init');call('iop_intc_init');timers=[]
 for i,b in enumerate(base):
  mode=[0,0x50,0x58,0x10,0x20,0x60][(scenario+i)%6]
  target=[0,1,7,63,65535,0xffffffff][(scenario//3+i)%6]
  count=[0,3,65534,0xfffffffe][(scenario+i)%4]
  call('iop_timers_mmio_write32',b+8,target);call('iop_timers_mmio_write32',b+4,mode);call('iop_timers_mmio_write32',b,count)
  timers.append([count,(mode&0x63ff)|0x400,target])
 for tick in range(144):
  expected_irq=scalar(timers);call('iop_timers_tick')
  assert call('iop_intc_mmio_read32',0x1f801070,scratch)==1
  actual_irq=read(scratch)
  assert actual_irq==expected_irq,(scenario,tick,hex(actual_irq),hex(expected_irq))
  assert call('iop_intc_mmio_write32',0x1f801070,0)==1
  if tick%19==0:
   got=snapshot()
   assert list(got)==[v for t in timers for v in t],(scenario,tick)
  if tick%31==0:
   i=rng.randrange(6);reg=rng.randrange(3);value=rng.getrandbits(32)
   if reg==1:value&=0x63ff
   call('iop_timers_mmio_write32',base[i]+reg*4,value)
   if reg==1:timers[i][0]=0;timers[i][1]=(value&0x63ff)|0x400
   else:timers[i][0 if reg==0 else 2]=value
  cases+=1
 assert snapshot()==[v for t in timers for v in t]
print('PASS',cases,'linked PPC IOP timer ticks, independent state/IRQ oracle and mixed MMIO',Path(a.elf).name)
