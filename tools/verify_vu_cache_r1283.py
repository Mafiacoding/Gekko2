"""Actual Wii ELF VU hot-prefix cache reads and SMC boundaries."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_block_r1272.py').read_text(),'verify_ppc_block_r1272.py','exec'))
import json
from unicorn import UC_HOOK_MEM_READ
reads=[0];active=[False]
def micro_read(uc,access,address,size,value,data):
 if active[0] and mc<=address<mc+16384:reads[0]+=size
u.hook_add(UC_HOOK_MEM_READ,micro_read);u.ctl_remove_cache(0x80000000,0x81800000)
setup();fresh_up=up1+(2<<6);put(0,lo,fresh_up)
assert block()==(2 if enabled else 0)
setup();put(0,lo,fresh_up);reads[0]=0;before=allocs;active[0]=True;got=block();active[0]=False
if enabled:
 assert got==2 and allocs==before
 warm=reads[0]
 # Changes beyond the compiled prefix must not invalidate or execute it.
 setup();put(0,lo,fresh_up);put(16,lo,up1);before=allocs
 assert block()==2 and allocs==before
 # Changes within the cached prefix must recompile and see the new operation.
 setup();put(0,lo,fresh_up);put(8,lo,(15<<21)|(2<<16)|(3<<11)|(4<<6)|0x2c)
 assert block()==2
 assert struct.unpack('>4I',bytes(u.mem_read(vf+64,16)))==(0xc0000000,)*4
 # Shorter budget cannot execute an old longer compiled prefix.
 setup();put(0,lo,fresh_up);assert block(1)==0
 print('VU_CACHE_MEASURE',json.dumps({'elf':Path(a.elf).name,'cached_pairs':2,'warm_micro_read_bytes':warm,'scope':'Actual ELF exact-word micro validation reads; no Wii FPS claim.'}))
print('PASS actual Wii ELF VU cached-prefix SMC/owner/budget behavior')
