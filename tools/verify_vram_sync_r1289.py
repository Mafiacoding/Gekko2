"""Exercise real linked PPC VRAM barriers with a synthetic deferred GPU resolver.
The resolver is injected PPC, not an emulated GX device or Wii GPU proof.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
u.mem_map(0x90000000,0x10000)
# Callback ABI: r3=opaque, r4=raw GS RAM, r5=size. Store LE RGBA 11,22,33,44.
ops=[]
for n,value in enumerate([0x11,0x22,0x33,0x44]):
    ops.extend([0x38c00000|value,0x98c40000|n]) # li r6,value; stb r6,n(r4)
ops += [0x38600001,0x4e800020]
u.mem_write(0x90000000,b''.join(struct.pack('>I',iw) for iw in ops))
u.mem_write(0x90000100,struct.pack('>2I',0x38600000,0x4e800020))
call('gs_mem_init');assert call('gs_mem_gpu_bind',0x90000000,0)==1
assert call('gs_mem_gpu_mark_pending')==1
assert call('gs_mem_gpu_pending')==1
assert call('gs_mem_read_psmct32',0,64,0,0)==0x44332211
assert call('gs_mem_gpu_pending')==0
assert call('gs_mem_gpu_mark_pending')==1
call('gs_mem_write_psmct32',0,64,0,0,0x78563412)
assert call('gs_mem_read_psmct32',0,64,0,0)==0x78563412
assert call('gs_mem_gpu_bind',0x90000100,0)==1
assert call('gs_mem_gpu_mark_pending')==1
assert call('gs_mem_read_psmct32',0,64,0,0)==0
call('gs_mem_write_psmct32',0,64,0,0,0xdeadbeef)
assert bytes(u.mem_read(syms['g_gs_mem'],4))==b'\x12\x34\x56\x78'
assert call('gs_mem_get')==0
assert call('gs_mem_gpu_bind',0x90000000,0)==0 # cannot replace failed owner
assert call('gs_mem_gpu_pending')==1
# Repair resolver to succeed, retry without losing dirty ownership.
u.mem_write(0x90000100,b''.join(struct.pack('>I',iw) for iw in ops))
u.ctl_remove_cache(0x90000100,0x90000100+len(ops)*4)
assert call('gs_mem_sync')==1
assert call('gs_mem_read_psmct32',0,64,0,0)==0x44332211
assert call('gs_mem_gpu_bind',0,0)==1
assert call('gs_mem_gpu_mark_pending')==0
# Exact GX-tile import into a page-crossing PS2 target. Eight arguments fit
# registers, remaining 3 follow the PPC ABI at SP+8, +12, +16.
src=0x90001000;data=bytearray(128)
for y in range(4):
 for x in range(8):
  o=(x//4)*64+y*8+(x%4)*2
  data[o:o+2]=bytes([255,x+1]);data[o+32:o+34]=bytes([y+11,23])
u.mem_write(src,bytes(data))
def unpack(cap=128,bp=64,width=8,alpha=128):
    args=[syms['g_gs_mem'],4*1024*1024,src,cap,bp,128,63,31]
    for i,v in enumerate(args):u.reg_write(UC_PPC_REG_3+i,v)
    u.reg_write(UC_PPC_REG_1,0x81780000)
    u.mem_write(0x81780008,struct.pack('>3I',width,4,alpha))
    u.reg_write(UC_PPC_REG_LR,0x817ff000)
    u.emu_start(syms['gs_gx_unpack_psmct32'],0x817ff000,count=10000000)
    assert u.reg_read(UC_PPC_REG_PC)==0x817ff000
    return u.reg_read(UC_PPC_REG_3)
assert unpack()==1
for y in range(4):
 for x in range(8):assert call('gs_mem_read_psmct32',64,128,63+x,31+y)==0x80170000|((y+11)<<8)|(x+1)
before=bytes(u.mem_read(syms['g_gs_mem'],4*1024*1024))
for kwargs in [{'cap':127},{'bp':0xffffffff},{'width':7},{'alpha':256}]:
 assert unpack(**kwargs)==0
 assert bytes(u.mem_read(syms['g_gs_mem'],len(before)))==before
print('PASS actual PPC VRAM deferred read/write/raw barriers, failure ownership/retry and 32-pixel tiled readback; GX hardware unverified')
# Execute the capture/readback orchestration in the real ELF, mocking only
# libogc GPU/cache services. This proves ordering, not physical GPU output.
from unicorn import UC_HOOK_CODE
u.mem_map(0x90200000,2*1024*1024)
initialized_addr=next(int(l.split()[0],16) for l in nm.splitlines() if len(l.split())==3 and l.split()[1]=='b' and l.split()[2]=='initialized')
u.mem_write(initialized_addr,struct.pack('>I',1))
u.mem_write(syms['efb_width'],struct.pack('>I',640));u.mem_write(syms['efb_height'],struct.pack('>I',512))
services=['memalign','DCFlushRange','GX_SetCopyFilter','GX_SetTexCopySrc','GX_SetTexCopyDst','GX_CopyTex','GX_DrawDone','DCInvalidateRange']
addresses={syms[n]:n for n in services};events=[]
def service(uc,address,size,user):
    name=addresses.get(address)
    if not name:return
    args=[uc.reg_read(UC_PPC_REG_3+i) for i in range(3)];events.append((name,args))
    if name=='GX_CopyTex':uc.mem_write(args[0],bytes(data))
    uc.reg_write(UC_PPC_REG_3,0x90200000 if name=='memalign' else 0)
    uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
handle=u.hook_add(UC_HOOK_CODE,service)
call('gs_mem_init')
assert call('gs_gx_capture_vram_psmct32',64,128,63,31,8,4,128)==1
assert call('gs_mem_gpu_pending')==1
assert [n for n,_ in events]==['memalign','DCFlushRange','GX_SetCopyFilter','GX_SetTexCopySrc','GX_SetTexCopyDst','GX_CopyTex']
assert call('gs_mem_read_psmct32',64,128,63,31)==0x80170b01
assert [n for n,_ in events][-2:]==['GX_DrawDone','DCInvalidateRange']
assert events[-1][1][:2]==[0x90200000,128]
assert call('gs_mem_gpu_pending')==0
before=len(events)
assert call('gs_gx_capture_vram_psmct32',64,128,0,0,640,516,128)==0
assert len(events)==before
u.hook_del(handle)
print('PASS actual ELF capture orchestration: flush -> queued texture copy -> deferred GPU wait -> CPU invalidate -> VRAM import; GPU/cache services mocked')
