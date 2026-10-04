import struct,sys
from pathlib import Path
import numpy as np
from PIL import Image
b=Path(sys.argv[1]).read_bytes();o=8;blocks={}
while o+8<=len(b):
 tag=b[o:o+4];n=struct.unpack_from('<I',b,o+4)[0];blocks[tag]=b[o+8:o+8+n];o+=8+n
regs=struct.unpack('<19Q',blocks[b'GS00']);disp=regs[9];bp=(disp&511)*2048;bw=((disp>>9)&63)*64
x,y=np.meshgrid(np.arange(640,dtype=np.uint32),np.arange(256,dtype=np.uint32));bx=(x&63)>>3;by=(y&31)>>3
block=(bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2)
word=(x&1)|((y&1)<<1)|((x&6)<<1)|((y&6)<<3)
off=bp*4+((y//32)*max(bw//64,1)+x//64)*8192+block*256+word*4
v=np.frombuffer(blocks[b'GSM0'],dtype=np.uint8);rgb=np.stack([v[off],v[off+1],v[off+2]],axis=-1)
Image.fromarray(rgb).resize((1280,512)).save(sys.argv[2]);print('RGB pixels',np.any(rgb,axis=-1).sum())
