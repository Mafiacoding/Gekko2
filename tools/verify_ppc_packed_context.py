"""Exercise PACKED TEX0/CLAMP and context switch rasterization in the Wii ELF."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_spr.py').read_text(),'verify_ppc_spr.py','exec'))
from unicorn.ppc_const import UC_PPC_REG_MSR
u.reg_write(UC_PPC_REG_MSR,u.reg_read(UC_PPC_REG_MSR)|0x2000)
packet=0x81740000
# Clear SPR/MFIFO bindings from the preceding DMA test.
call('gif_init');call('gs_mem_init')
def feed(blob):
 u.mem_write(packet,blob);call('gif_process_quadwords',2,packet,len(blob)//16)
def ad(reg,lo,hi=0):
 feed(struct.pack('<4I',0x8001,1<<28,14,0)+struct.pack('<4I',lo,hi,reg,0))
for ctx in [0,1]:
 tex=2000+ctx*100;value=0x80332211 if not ctx else 0x80665544
 call('gs_mem_write_psmct32',tex*64,64,3,2,value)
 clamp=2|(2<<2)|(3<<4)|(3<<14)|(2<<24)|(2<<34)
 tag=struct.pack('<4I',0x8001,2<<28,(6+ctx)|((8+ctx)<<4),0)
 feed(tag);feed(struct.pack('<4I',tex|(1<<14)|(2<<26)|(2<<30),(1<<2)|(1<<3),0xdeadbeef,0xfeedface));feed(struct.pack('<4I',clamp&0xffffffff,clamp>>32,~0&0xffffffff,~0&0xffffffff))
 ad(0x4c+ctx,1<<16);ad(0x18+ctx,0,0);ad(0x40+ctx,7<<16,7<<16)
 ad(0,6|(1<<4)|(1<<8)|(ctx<<9));ad(1,0x80808080,0x3f800000);ad(3,0);ad(5,0);ad(3,0);ad(5,64|(64<<16))
 assert call('gs_mem_read_psmct32',0,64,1,1)==value
call('gif_init');call('gs_mem_init')
for ctx in [0,1]:
 ad(0x4c+ctx,1<<16);ad(0x40+ctx,31<<16,31<<16);ad(0x18+ctx,(32 if not ctx else 128)<<4,(16 if not ctx else 112)<<4)
for ctx,x,y in [(0,2,2),(1,10,10),(0,20,20)]:
 ox,oy=(32,16) if not ctx else (128,112);value=0x800000ff if not ctx else 0x8000ff00
 ad(0,6|(ctx<<9));ad(1,value);ad(5,((x+ox)<<4)|(((y+oy)<<4)<<16));ad(5,((x+ox+4)<<4)|(((y+oy+4)<<4)<<16))
 assert call('gs_mem_read_psmct32',0,64,x+1,y+1)==value
print('PASS 5 actual Wii ELF rendered-image checks: packed textures/clamp, split packets and alternating context offsets')
# New metadata parser must decode the original little-endian IRX on PPC.
b=bytearray(160);b[:6]=b'\x7fELF\x01\x01';struct.pack_into('<I',b,32,52);struct.pack_into('<HH',b,46,40,1);struct.pack_into('<I',b,56,0x70000080);struct.pack_into('<II',b,68,96,40);struct.pack_into('<H',b,120,0x208);b[122:129]=b'mcserv\0';u.mem_write(packet,bytes(b))
assert call('iop_module_metadata',packet,len(b),packet+256,packet+320)==1
assert bytes(u.mem_read(packet+256,7))==b'mcserv\0'
assert bytes(u.mem_read(packet+320,2))==b'\x02\x08'
assert call('iop_module_metadata',packet,110,packet+256,packet+320)==0
print('PASS 4 actual Wii ELF IRX metadata checks: LE ELF, module name, native version and truncation')
