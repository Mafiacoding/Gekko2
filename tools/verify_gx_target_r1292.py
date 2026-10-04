"""Validate the actual PPC constant-time bound against exhaustive pixel addresses."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_gx_flat_r1290.py').read_text(),'verify_gx_flat_r1290.py','exec'))
import random
rng=random.Random(1292)
bx=[0,1,4,5,16,17,20,21];by=[0,2,8,10];wx=[0,1,4,5,8,9,12,13];wy=[0,2,16,18,32,34,48,50]
def offset(bp,bw,x,y):return bp*4+(y//32*(bw//64)+x//64)*8192+(bx[x%64//8]+by[y%32//8])*256+(wx[x%8]+wy[y%8])*4
for n in range(256):
 w=rng.randrange(1,33)*4;h=rng.randrange(1,33)*4
 bw=rng.randrange(1,33)*64;x=rng.randrange(2049-w);y=rng.randrange(2049-h)
 end=max(offset(0,bw,x+i,y+j)for j in range(h)for i in range(w))
 bp=rng.getrandbits(32)if n%3==0 else rng.randrange(1048576)
 if n%3==2:bp=max(0,(4194304-end)//4)+rng.choice([-1,0,1]);bp=max(0,bp)
 want=int(all(offset(bp,bw,x+i,y+j)<=4194300 for j in range(h)for i in range(w)))
 assert invoke('gs_gx_target_valid',4194304,bp,bw,x,y,w,h)==want,(n,bp,bw,x,y,w,h)
for args in [(4194303,0,64,0,0,4,4),(4194304,0,0,0,0,4,4),(4194304,0,65,0,0,4,4),(4194304,0,64,0xffffffff,0,4,4),(4194304,0,64,0,0,0,4),(4194304,0,64,0,0,641,4)]:assert invoke('gs_gx_target_valid',*args)==0
print('PASS 256 actual PPC rectangle bounds against exhaustive table-address oracle, VRAM-end and hostile parameter cases')
