"""Actual PPC GS byte-width bounds, including 32-bit check overflow."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
from unicorn import UcError
guard=syms['g_gs_mem']-16
sentinel=bytes.fromhex('102030405060708090a0b0c012345678')
u.mem_write(guard,sentinel)
# R1289 places live GPU ownership fields directly before VRAM. A bounds
# fixture must not replace those pointers/flags with the arbitrary canary.
for field in ('g_gpu_opaque','g_gpu_pending','g_gpu_resolver','g_sync_active'):
 if field in syms:u.mem_write(syms[field],bytes(4))
sentinel=bytes(u.mem_read(guard,16))
if 'R1285-' in Path(a.elf).name:
 value=call('gs_mem_read_psmct32',0x3fffffff,64,0,0)
 assert value==0x78563412,hex(value)
 call('gs_mem_write_psmct32',0x3fffffff,64,0,0,0xaabbccdd)
 assert bytes(u.mem_read(guard,16))!=sentinel
 print('REPRODUCED R1285 invalid GS read/write accessed bytes before VRAM after off+4 wrapped');raise SystemExit(0)
checks=0;before=bytes(u.mem_read(syms['g_gs_mem'],4*1024*1024))
formats=[('psmct32',[0x3fffffff,64,0,0],[1048575,64,0,0],4),
 ('psmct32_swizzled',[0x7ffff,64,63,31],[511,64,63,31],4),
 ('psmct16',[0x3fffffff,64,8,0],[1048575,64,8,0],2),
 ('psmct16s',[0x3fffffff,64,8,0],[1048575,64,8,0],2)]
for psm in [0,1,2,10]:
 width=2 if psm in [2,10] else 3 if psm==1 else 4
 # Color address XOR0x1800 makes the final depth address.
 bad=[0x3ffff9ff,64,8 if width==2 else 0,0,psm]
 good=[((4*1024*1024-4)^6144)//4,64,8 if width==2 else 0,0,psm]
 formats.append(('z',bad,good,width))
for name,bad,good,width in formats:
 if 'gs_mem_read_'+name not in syms or 'gs_mem_write_'+name not in syms:
  print('SKIP unreferenced compatibility API removed by ELF linker:',name);continue
 for args in [bad,[bad[0]-1]+bad[1:]]:
  assert call('gs_mem_read_'+name,*args)==0,(name,args)
  call('gs_mem_write_'+name,*args,0xaabbccdd)
  assert bytes(u.mem_read(syms['g_gs_mem'],len(before)))==before,(name,args)
  assert bytes(u.mem_read(guard,16))==sentinel,(name,args,'prefix guard')
  checks+=2
 call('gs_mem_write_'+name,*good,0xaabbccdd)
 assert call('gs_mem_read_'+name,*good)==(0xaabbccdd&((1<<(width*8))-1)),name
 # All final accesses except 24-bit end at the exact physical VRAM end.
 off=4*1024*1024-(2 if width==2 else 4)
 assert bytes(u.mem_read(syms['g_gs_mem']+off,width))==(0xaabbccdd&((1<<(width*8))-1)).to_bytes(width,'little')
 checks+=2
 u.mem_write(syms['g_gs_mem'],before)
print('PASS',checks,'actual Wii ELF GS bounds cases: invalid overflow addresses rejected, untouched VRAM, final valid 16/24/32-bit color/depth bytes')
