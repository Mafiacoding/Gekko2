"""Linked-PPC digest reuse cost; CPU instruction count, no GPU/Wii FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
initial=int.from_bytes(u.mem_read(syms['gekko2_optimization_mask'],4),'big')
word(syms['gekko2_optimization_mask'],initial | (1<<8))
call('gs_mem_init');ptr=0x816e0000;hashes=[]
total[0]=0;active[0]=True
for _ in range(8):
 assert call('gs_mem_hash_range',0,16384,ptr)==1
 hashes.append(bytes(u.mem_read(ptr,8)).hex())
active[0]=False
assert len(set(hashes))==1
print(json.dumps({'ppc_instructions':total[0],'hashes':hashes,'calls':8,'bytes_per_call':16384}))
