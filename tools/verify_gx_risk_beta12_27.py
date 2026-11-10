"""Linked PPC risk gate and command reduction. No real GPU edge/FPS claim."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_gx_common_beta12_26.py').read_text(),'verify_gx_common_beta12_26.py','exec'))
with tempfile.TemporaryDirectory() as td:
 src=Path(td)/'risk.c';exe=Path(td)/'risk.elf';raw=Path(td)/'risk.bin'
 src.write_text('''#include "core/hw/gs_gx.h"
int risk(const gs_gx_native_color *c,const unsigned *rgba,const gs_gx_pipeline *p,gs_gx_texel_fn sample,const int *xy,const unsigned *zs,unsigned scan){
 float uv[6]={0,0,0,0,0,0};
 return gs_gx_draw_native_triangle_ex(0,0,64,2,2,18,18,xy,uv,0,rgba,4,4,1,1,0,scan,sample,p,zs,c);
}
''')
 subprocess.run([cc,'-O2','-msdata=none','-mcpu=750','-G0','-ffreestanding','-nostdlib','-I'+str(root/'include'),'-Wl,-Ttext=0x817e0000','-Wl,-e,risk','-Wl,--defsym,gs_gx_draw_native_triangle_ex='+hex(syms['gs_gx_draw_native_triangle_ex']),str(src),'-o',str(exe)],check=True)
 subprocess.run([oc,'-O','binary','-j','.text','-j','.rodata','-j','.sdata2','-j','.data',str(exe),str(raw)],check=True)
 u.mem_write(0x817e0000,raw.read_bytes());syms['test_risk']=0x817e0000
option=2
assert call('gekko2_experimental_available')&(1<<option)
assert call('gekko2_experimental_enabled',option)==0
p[6]=0;p[9]=0;p[12]=0;p[18]=0;p[16]=0;p[4]=1;p[14]=0
u.mem_write(pipe,struct.pack('>22I',*p))
def stat(n):
 call('gs_gx_extended_stat',n)
 return (u.reg_read(UC_PPC_REG_3)<<32)|u.reg_read(UC_PPC_REG_4)
def draw(scan=0):
 before=len(begins);scissors.clear()
 assert call('test_risk',colorptr,rgbaptr,pipe,sample,base,zs,scan)==1
 return len(begins)-before
normal=draw();assert normal>1 and stat(13)==0
alpha_before=bytes(u.mem_read(alpha,64*64))
z_before=[call('gs_mem_read_z',16384,64,x,4,0) for x in range(3,13)]
assert call('gekko2_experimental_request',option,1)==1
assert call('gekko2_experimental_enabled',option)==0 and draw()==normal
# Isolated active-gate test. Real UI applies only after the existing cold-boot drain.
call('gekko2_experimental_apply_cold_boot');assert call('gekko2_experimental_enabled',option)==1
before=stat(13);saved=stat(15)
assert draw()==1 and stat(13)==before+1 and stat(15)==saved+(normal-1)*3
assert (2,2,17,17) in scissors and (2,2,20,20) not in scissors,'must not draw upload padding'
assert bytes(u.mem_read(alpha,64*64))==alpha_before
assert [call('gs_mem_read_z',16384,64,x,4,0) for x in range(3,13)]==z_before
# Requesting OFF cannot alter a paused/running session.
assert call('gekko2_experimental_request',option,0)==1
assert call('gekko2_experimental_enabled',option)==1 and draw()==1
call('gekko2_experimental_apply_cold_boot');assert draw()==normal
call('gekko2_experimental_request',option,1);call('gekko2_experimental_apply_cold_boot')
# PABE, alpha, DATE, Z islands and scan masks retain GS row permissions.
before=stat(13);rejected=stat(14)
p[6]=1;p[9]=2;p[12]=1;u.mem_write(pipe,struct.pack('>22I',*p));assert draw()>1 and stat(13)==before
p[6]=0;p[12]=0;p[18]=1;p[19]=6;p[20]=128;p[21]=0
u.mem_write(pipe,struct.pack('>22I',*p));assert draw()>1 and stat(13)==before
p[18]=0;p[16]=1;p[17]=1;u.mem_write(pipe,struct.pack('>22I',*p));assert draw()>1 and stat(13)==before
p[16]=0;p[4]=2;u.mem_write(pipe,struct.pack('>22I',*p));call('gs_mem_write_z',16384,64,4,4,0,129)
assert draw()>1 and stat(13)==before
p[4]=1;u.mem_write(pipe,struct.pack('>22I',*p))
for scan in [2,3]:
 assert draw(scan)>1 and stat(13)==before
 for x,y,w,h in scissors:
  if x>=2 and y>=2 and w<20:assert h==1 and (y&1)==(1 if scan==2 else 0)
assert stat(14)==rejected+6
# Risk never forces an unsupported framebuffer mask or missing GX gate.
p[14]=1;u.mem_write(pipe,struct.pack('>22I',*p));before_begins=len(begins)
assert call('test_risk',colorptr,rgbaptr,pipe,sample,base,zs,0)==0 and len(begins)==before_begins
p[14]=0;u.mem_write(pipe,struct.pack('>22I',*p));word(syms['render_enabled'],0)
assert call('test_risk',colorptr,rgbaptr,pipe,sample,base,zs,0)==0 and len(begins)==before_begins
print(f'PASS linked PPC GX risk: OFF={normal} submissions, ON=1 for eligible triangle; cold-boot request isolation, unpadded bbox, exact alpha/Z metadata, PABE/ATE/DATE/Z/SCANMSK fallbacks and unsupported-state gates; actual GPU edge coverage and Wii FPS unverified')
