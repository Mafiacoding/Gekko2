#!/usr/bin/env python3
"""Execute GS ACK/IMR, Q-LOD/MTBA, VIF SQ and ENVX on the delivered Wii ELF.
Synthetic input only; platform services follow the existing Unicorn fixture.
"""
from pathlib import Path
source=Path(__file__).with_name('verify_beta12_22_linked.py').read_text()
exec(compile(source.split('_,vo=')[0],'verify_beta12_22_linked.py','exec'))
_,gso=layout('core/hw/gs.h','gs_state_t',['csr','imr','siglblid'])
_,gfo=layout('core/hw/gif.h','gif_state_t',['ctx1_tex_mip_tbp','ctx1_tex_mip_tbw','ctx2_tex_mip_tbp','ctx2_tex_mip_tbw'])
_,vfo=layout('core/hw/vif.h','vif_state_t',['stall_words','irq_stalled','mark','stall_buffer'])
gs=call('gs_get_state');gif=call('gif_get_state');inp=ram+0x1000;checks=0
mask=syms['gekko2_optimization_mask'];word(mask,0)
def ad(reg,lo,hi=0):
 u.mem_write(inp,struct.pack('<8I',0x8001,1<<28,14,0,lo&0xffffffff,hi&0xffffffff,reg,0))
 call('gif_process_quadwords',2,inp,2)
def guest_reg(index,value):u.mem_write(state+index*16,struct.pack('>QQ',value,0))
def read_word(address):return int.from_bytes(u.mem_read(address,4),'big')
for alias in (0,0x80000000,0xa0000000):
 for opcode,width in ((40,8),(41,16),(43,32),(63,64)):
  setup();call('gs_init');call('ee_intc_init');word(mask,0)
  u.mem_write(gs+gso['csr'],struct.pack('>Q',3));word(state+off['pc'],0x80200000);word(state+off['next_pc'],0x80200004)
  guest_reg(1,alias|0x12001000);guest_reg(2,1);u.mem_write(ram+base,struct.pack('<I',(opcode<<26)|(1<<21)|(2<<16)))
  assert call('ee_core_step_n',1)==1
  assert bytes(u.mem_read(gs+gso['csr'],8))==struct.pack('>Q',2),(opcode,alias)
  assert call('ee_mem_read32',state,alias|0x12001000)==2
  checks+=1
for event in range(5):
 call('gs_init');call('ee_intc_init');assert call('gs_raise_event',event)==0
 # Clear the event's mask via a real EE SW, delivering its already-pending IRQ.
 call('ee_mem_write32',state,0xb2001010,0x1f00&~(1<<(event+8)))
 assert call('ee_intc_get_raise_count',0)==1
 call('ee_mem_write32',state,0xb2001010,0x1f00&~(1<<(event+8)))
 assert call('ee_intc_get_raise_count',0)==1;checks+=1
print(f'PASS linked GS narrow guest ACK/read aliases and pending-IMR edges: {checks}',flush=True)

# Q-varying natural GIF triangle, two distinct mip levels at the same draw.
call('gif_init');call('gs_mem_init')
ad(0x4c,1<<16);ad(6,1024|(1<<14)|(6<<26)|(2<<30),1|4|8);ad(0x14,(6<<2)|(2<<6));ad(8,0)
for reg,first in ((0x34,1),(0x36,4)):
 value=0
 for n in range(3):value|=((1024+(first+n)*256)|(1<<14))<<(n*20)
 ad(reg,value&0xffffffff,value>>32)
for level in range(7):call('gs_mem_write_psmct32',((1024+level*256)*64),64,0,0,0x80000000|level*17)
ad(0,3|16)
for x,y,q in ((0,0,0x3f800000),(8,0,0x3e000000),(0,8,0x3f800000)):
 ad(1,0x80808080,q);ad(2,0,0);ad(5,(x*16)|((y*16)<<16))
assert call('gs_mem_read_psmct32',0,64,1,1)==0x80000000
assert call('gs_mem_read_psmct32',0,64,6,1)==0x80000022
checks+=2
for context in (0,1):
 for psm,bpp in ((0,32),(1,32),(2,16),(10,16),(19,8),(20,4),(27,32),(36,32),(44,32)):
  call('gif_init');ad(0x15 if context else 0x14,(1<<9)|(6<<2));ad(7 if context else 6,16380|(3<<14)|(psm<<20)|(6<<26),3)
  offsets=gfo['ctx2_tex_mip_tbp' if context else 'ctx1_tex_mip_tbp'];strides=gfo['ctx2_tex_mip_tbw' if context else 'ctx1_tex_mip_tbw']
  expected=[];bp=16380;blocks=64*64*bpp//2048
  for n in range(3):bp=(bp+blocks)&16383;expected.append(bp);blocks=max(blocks//4,1)
  assert list(struct.unpack('>3I',u.mem_read(gif+offsets,12)))==expected
  assert struct.unpack('>3I',u.mem_read(gif+strides,12))==(64,64,64);checks+=1
print('PASS linked Q-dependent fragment mip selection and 18 color-format/context MTBA layouts',flush=True)

for channel,capacity in ((0,8),(1,16)):
 setup();call('vif_init');word(mask,0);vif=call('vif1_get_state' if channel else 'vif0_get_state')
 u.mem_write(inp,struct.pack('<I',0x87000001));call('vif1_process_tag_words' if channel else 'vif0_process_tag_words',channel,inp,1)
 assert read_word(vif+vfo['irq_stalled'])==1
 word(state+off['pc'],0x80200000);word(state+off['next_pc'],0x80200004);guest_reg(1,0xb0005000 if channel else 0xb0004000)
 u.mem_write(state+32,struct.pack('>QQ',0x0700123507001234,0x0700123707001236))
 u.mem_write(ram+base,struct.pack('<'+str(capacity+1)+'I',*([(0x1f<<26)|(1<<21)|(2<<16)]*(capacity+1))))
 for n in range(capacity):assert call('ee_core_step_n',1)==1
 assert read_word(vif+vfo['stall_words'])==capacity*4
 before=bytes(u.mem_read(state,size));payload=bytes(u.mem_read(vif+vfo['stall_buffer'],capacity*16));count=executed()
 for n in range(3):
  assert call('ee_core_step_n',1)==0
  assert read_word(state+off['pc'])==0x80200000+capacity*4 and executed()==count
  assert bytes(u.mem_read(vif+vfo['stall_buffer'],capacity*16))==payload
 assert call('vif_mmio_write32',0x10003c10 if channel else 0x10003810,8)==1
 assert call('ee_core_step_n',1)==1 and executed()==count+1
 assert read_word(vif+vfo['mark'])==0x1237;checks+=1
print('PASS linked VIF0/1 real SQ full-capacity retry, retained bytes and STC resume',flush=True)

call('iop_spu2_init');spu=call('spu2_mixer_get_ram');u.mem_write(spu+0x6000,b'\0\7'+b'\x77'*14)
base_spu=0x1f900000;voice=base_spu+16
for address,value in ((base_spu+0x1cc,0),(base_spu+0x1ce,0x3000),(voice+6,0x7f00),(base_spu+0x1a0,2),(voice+10,0x4000)):
 assert call('iop_spu2_mmio_write16',address,value)==1
call('spu2_mixer_render_frames',inp,1)
assert bytes(u.mem_read(spu+0x800,2))==b'\0\x38';checks+=1
print(f'PASS linked writable ENVX changes post-ADSR capture; combined new accuracy checks={checks}',flush=True)
