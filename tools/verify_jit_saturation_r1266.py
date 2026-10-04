"""Generated PPC unsigned saturated add, plus PLZCW; independent integer oracle."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_jit_plzcw_r1266.py').read_text(),'verify_jit_plzcw_r1266.py','exec'))
sat_cases=0
for bits,sa in [(32,0x10),(16,0x14),(8,0x18)]:
 maximum=(1<<bits)-1
 for i in range(2400):
  rs=i%32;rt=(i*7+3)%32;rd=[rs,rt,0,rng.randrange(32)][i%4]
  regs=[rng.getrandbits(64) for _ in range(68)];regs[:2]=[0,0]
  if i<1400:
   values=[0,1,maximum,maximum-1,maximum//2,maximum//2+1]
   for reg,offset in [(rs,0),(rt,1)]:
    if reg:
     value=sum(values[(i+n+offset)%len(values)]<<(n*bits) for n in range(128//bits));regs[reg*2]=value&((1<<64)-1);regs[reg*2+1]=value>>64
  a=regs[rs*2]|regs[rs*2+1]<<64;b=regs[rt*2]|regs[rt*2+1]<<64;expect=regs[:]
  result=sum(min(maximum,((a>>(n*bits))&maximum)+((b>>(n*bits))&maximum))<<(n*bits) for n in range(128//bits))
  if rd:expect[rd*2]=result&((1<<64)-1);expect[rd*2+1]=result>>64
  ctx=Context();assert lib.ppc_dynarec_init(C.byref(ctx),1)==0
  assert lib.ppc_dynarec_translate_one(C.byref(ctx),(28<<26)|(rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|0x28)==0
  lib.ppc_dynarec_finalize(C.byref(ctx));code=struct.pack('>'+str(ctx.used)+'I',*ctx.code[:ctx.used]);lib.ppc_dynarec_free(C.byref(ctx))
  u.mem_write(0x10000,code);u.ctl_remove_cache(0x10000,0x12000);u.mem_write(0x20000,struct.pack('>68Q',*regs)+bytes(0x2000))
  for n in range(32):u.reg_write(UC_PPC_REG_0+n,0)
  u.reg_write(UC_PPC_REG_1,0x32000);u.reg_write(UC_PPC_REG_3,0x20000);u.reg_write(UC_PPC_REG_LR,0x11000)
  u.emu_start(0x10000,0x11000,count=500)
  got=struct.unpack('>68Q',u.mem_read(0x20000,544));assert tuple(expect)==got,(bits,i,rs,rt,rd);sat_cases+=1
print('PASS',sat_cases,'generated PPC unsigned saturation cases')
(out/'jit_saturation_r1266.json').write_text(json.dumps({'cases':sat_cases,'errors':0,'scope':'Generated PPC blocks; no Wii FPS measurement.'},indent=2))
