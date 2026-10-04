"""Exercise the Wii ELF's native CLUT loader on a big-endian PowerPC CPU."""
from pathlib import Path
import re
# Reuse the existing ELF loading and native VRAM checks, including CLI options.
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(), 'verify_ppc_gs_memory.py', 'exec'))
readelf=str(Path(a.nm).with_name('powerpc-eabi-readelf'))
dwarf=subprocess.check_output([readelf,'--debug-dump=info',a.elf],text=True)
m=re.search(r'DW_AT_name[^\n]*: clut_cache\n(?:(?!DW_AT_name)[^\n]*\n){0,9}?[^\n]*DW_AT_data_member_location:\s*(\d+)',dwarf)
if not m:raise RuntimeError('No authoritative CLUT cache member offset in ELF DWARF')
cache=syms['g_gif']+int(m.group(1))
def cache_value(i):return struct.unpack('>I',bytes(u.mem_read(cache+i*4,4)))[0]
for y in range(2):
 for x in range(8):call('gs_mem_write_psmct32',6400,64,x,y,0x80010000+y*8+x)
call('gs_load_clut',0x14,100,0,0,3,1)
for i in range(16):assert cache_value(48+i)==0x80010000+i
call('gs_mem_write_psmct32',6400,64,0,1,0x80123456)
call('gs_load_clut',0x14,100,0,0,3,0)
assert cache_value(56)==0x80010008
call('gs_load_clut',0x14,100,0,0,3,2)
assert cache_value(56)==0x80123456
call('gs_mem_write_psmct32',6400,64,0,1,0x80abcdef)
call('gs_load_clut',0x14,100,0,0,3,4)
assert cache_value(56)==0x80123456
print('PASS 19 actual Wii ELF CLUT checks: native 8x2 palette, CSA destination and CLD reuse')
