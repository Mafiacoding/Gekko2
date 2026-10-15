"""Linked-PPC zero-area triangle replay; exact queue and diagnostic results."""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'layout.c';obj=Path(d)/'layout.o';raw=Path(d)/'layout.bin'
 src.write_text('#include <stddef.h>\n#include "core/hw/gif.h"\nconst unsigned layout[]={offsetof(gif_state_t,prim),offsetof(gif_state_t,tri_vseq)};\n')
 subprocess.run([cc,'-O2','-G0','-mcpu=750','-I'+str(root/'include'),'-c',str(src),'-o',str(obj)],check=True)
 subprocess.run([oc,'-O','binary','-j','.rodata',str(obj),str(raw)],check=True)
 prim_off,seq_off=struct.unpack('>2I',raw.read_bytes())
call('gif_init');before_vram=bytes(u.mem_read(syms['g_gs_mem'],4*1024*1024));word(syms['g_gif']+prim_off,3|8)
total[0]=0;active[0]=True
for n in range(600):call('apply_xyz2_kick',0,0,0,1)
active[0]=False
assert before_vram==bytes(u.mem_read(syms['g_gs_mem'],4*1024*1024))
assert int.from_bytes(u.mem_read(syms['g_gif']+seq_off,4),'big')==600
# submitted and degenerate are enum slots 0 and 1.
assert call('gif_get_gouraud_stat',0)==0 and u.reg_read(UC_PPC_REG_4)==200
assert call('gif_get_gouraud_stat',1)==0 and u.reg_read(UC_PPC_REG_4)==200
print('TRIANGLE_COST',json.dumps({'PPC_instructions':total[0],'vertex_kicks':600,'degenerate':200,'VRAM_changes':0}))
print('PASS zero-area queue advances and diagnostic counts')
