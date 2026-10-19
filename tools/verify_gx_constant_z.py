#!/usr/bin/env python3
"""Linked PPC depth oracle and executed-instruction count; not Wii timing."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
import random, json
with tempfile.TemporaryDirectory() as td:
    src=Path(td)/'probe.c'; elf=Path(td)/'probe.elf'; raw=Path(td)/'probe.bin'
    src.write_text('''
extern unsigned native_fragment_z(int,int,const int *,const unsigned *,double);
unsigned probe(int x,int y,const int *v,const unsigned *z){
 int area=(v[4]-v[0])*(v[3]-v[1])-(v[5]-v[1])*(v[2]-v[0]);
 return native_fragment_z(x,y,v,z,1.0/(double)area);
}
''')
    subprocess.run([cc,'-O1','-ffp-contract=off','-mcpu=750','-G0','-ffreestanding','-nostdlib',
      '-Wl,-Ttext=0x817c0000','-Wl,-e,probe',
      '-Wl,--defsym,native_fragment_z='+hex(syms['native_fragment_z']),str(src),'-o',str(elf)],check=True)
    subprocess.run([oc,'-O','binary','-j','.text','-j','.rodata',str(elf),str(raw)],check=True)
    u.mem_write(0x817c0000,raw.read_bytes());syms['probe_z']=0x817c0000
xy_ptr=0x817a0000;z_ptr=xy_ptr+32
def edge(ax,ay,bx,by,x,y):return (x-ax)*(by-ay)-(y-ay)*(bx-ax)
rng=random.Random(33128);counts={'constant':0,'varying':0};checks=0
for k in range(512):
    # Both windings, nonzero area, interior and exterior samples, extreme Z.
    xy=[2,3,30,3,2,29]
    if k&1:xy=xy[4:6]+xy[2:4]+xy[:2]
    x=rng.randrange(-8,40);y=rng.randrange(-8,40)
    c=[0,1,0xffffff,0x1000000,0x7fffffff,0x80000000,0xffffffff][k%7]
    z=[c]*3 if k<256 else [rng.randrange(1<<32) for _ in range(3)]
    inv=1.0/edge(*xy[:4],*xy[4:])
    b=[edge(*xy[2:6],x,y)*inv,edge(*xy[4:6],*xy[:2],x,y)*inv,edge(*xy[:4],x,y)*inv]
    value=b[0]*z[0]+b[1]*z[1]+b[2]*z[2]
    want=0 if value<0 else 0xffffffff if value>=4294967295.0 else int(value+0.5)
    u.mem_write(xy_ptr,struct.pack('>6i',*xy));u.mem_write(z_ptr,struct.pack('>3I',*z))
    active[0]=True;total[0]=0
    got=call('probe_z',x&0xffffffff,y&0xffffffff,xy_ptr,z_ptr)
    active[0]=False
    assert got==want,(k,x,y,z,got,want)
    counts['constant' if k<256 else 'varying']+=total[0];checks+=1
print(json.dumps({'checks':checks,'executed_ppc_instructions':counts,
 'scope':'actual linked PPC depth interpolation; synthetic inputs; no Wii FPS measurement'}))
