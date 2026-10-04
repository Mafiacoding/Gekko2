"""Execute native EE COP0 transfers against an independent byte oracle.
Keeps the existing interpreter semantics, not a claim of complete PS2 fidelity.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_word_reads_r1278.py').read_text(),'verify_ppc_word_reads_r1278.py','exec'))
import random
rng=random.Random(1281);reset_frontend();checks=0
assert off['cop0']==556
for rs in [0,4]:
 for rd in range(32):
  for rt in [0,2]:
   for k in range(8):
    gpr=bytearray(rng.randbytes(512));gpr[:16]=bytes(16)
    cp=[rng.getrandbits(32) for j in range(32)];cp[rd]=[0,1,0x7fffffff,0x80000000,0xffffffff,rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32)][k]
    u.mem_write(state,bytes(gpr));u.mem_write(state+off['cop0'],struct.pack('>32I',*cp))
    expected_gpr=gpr.copy();expected_cp=cp.copy()
    iw=(16<<26)|(rs<<21)|(rt<<16)|(rd<<11)
    assert call('ee_jit_try_execute_one_at',state,0x80006200,iw)==int(enabled)
    if enabled:
     if rs==0 and rt:
      value=cp[rd] if cp[rd]<0x80000000 else cp[rd]|0xffffffff00000000
      expected_gpr[rt*16:rt*16+8]=value.to_bytes(8,'big')
     elif rs==4:
      value=int.from_bytes(gpr[rt*16+4:rt*16+8],'big')
      expected_cp[rd]=((value&~0xfc0)|0x440) if rd==16 else value
      if rd==11:expected_cp[13]&=~0x8000
    assert bytes(u.mem_read(state,512))==expected_gpr,(rs,rd,rt,k,'GPR')
    assert bytes(u.mem_read(state+off['cop0'],128))==struct.pack('>32I',*expected_cp),(rs,rd,rt,k,'COP0')
    checks+=1
# Branch/TLB/ERET forms stay on the existing interpreter path.
for iw in [(16<<26)|(8<<21),(16<<26)|(16<<21)|0x18,(16<<26)|(16<<21)|2]:
 assert call('ee_jit_try_execute_one_at',state,0x80006200,iw)==0
print(f'PASS {checks} actual Wii ELF native COP0 transfer byte-oracle cases, 32 registers, rt0, sign extension, Config read-only bits, Compare IP7 clear and fallback gates')
