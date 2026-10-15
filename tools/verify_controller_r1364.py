"""Compare the actual linked Wii PPC controller mappings with native C.
Hardware, Bluetooth, USB and firmware are not executed by this fixture.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0],'ppc_loader','exec'))
from unicorn import UC_HOOK_CODE
import ctypes,random,tempfile
root=Path(__file__).resolve().parents[1]
def memory_helpers(uc,address,size,user):
 if address==syms.get('memset'):
  dst=uc.reg_read(UC_PPC_REG_3);value=uc.reg_read(UC_PPC_REG_3+1)&255;n=uc.reg_read(UC_PPC_REG_3+2)
  uc.mem_write(dst,bytes([value])*n)
 elif address==syms.get('memcpy'):
  dst=uc.reg_read(UC_PPC_REG_3);src=uc.reg_read(UC_PPC_REG_3+1);n=uc.reg_read(UC_PPC_REG_3+2)
  uc.mem_write(dst,bytes(uc.mem_read(src,n)))
 else:return
 uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,memory_helpers)
pad=0x81600000;samples=0x81600100;focus=0x81601000;guest=0x81602000
def pad_be(raw):
 return struct.pack('>HH',*struct.unpack('<HH',raw[:4]))+raw[4:]
def focus_be(raw):
 return b''.join(pad_be(raw[i:i+12]) for i in range(0,168,12))+struct.pack('>H',struct.unpack('<H',raw[168:170])[0])+raw[170:172]+struct.pack('>i',struct.unpack('<i',raw[172:176])[0])
rng=random.Random(1364);checked=0
with tempfile.TemporaryDirectory() as tmp:
 lib=Path(tmp)/'controller.so'
 subprocess.run(['cc','-O2','-shared','-fPIC','-I'+str(root/'include'),str(root/'source/hw/controller_input.c'),'-o',str(lib)],check=True)
 native=ctypes.CDLL(str(lib))
 for _ in range(600):
  lo=rng.randrange(-128,0);mid=rng.randrange(0,128);hi=rng.randrange(mid+1,256);pos=rng.randrange(-200,400);invert=rng.randrange(2)
  expected=native.controller_axis(pos,lo,mid,hi,invert)&255
  assert call('controller_axis',pos&0xffffffff,lo&0xffffffff,mid,hi,invert)==expected
  checked+=1
 for _ in range(80):
  for name,values in [
    ('controller_map_gc',[rng.randrange(0x2000),*(rng.randrange(-128,128) for i in range(4)),rng.randrange(256),rng.randrange(256)]),
    ('controller_map_remote',[rng.randrange(0x40000),rng.randrange(2),rng.randrange(256),rng.randrange(256)]),
    ('controller_map_classic',[rng.getrandbits(32),rng.randrange(3),*(rng.randrange(256) for i in range(4))]),
    ('controller_map_drc',[rng.randrange(65536),rng.randrange(256),*(rng.randrange(-100,101) for i in range(4))])]:
   out=ctypes.create_string_buffer(12)
   getattr(native,name)(out,*[ctypes.c_uint(v&0xffffffff) for v in values])
   call(name,pad,*[v&0xffffffff for v in values])
   assert bytes(u.mem_read(pad,12))==pad_be(out.raw),(name,values,out.raw.hex(),bytes(u.mem_read(pad,12)).hex())
   checked+=1
 # All standard kinds, source transitions, full-pad and legacy session chords.
 raw=ctypes.create_string_buffer(13*12);nf=ctypes.create_string_buffer(176);native.controller_focus_init(nf);call('controller_focus_init',focus)
 for index in range(100):
  for i in range(13):native.controller_neutral(ctypes.byref(raw,i*12))
  active=index%13
  if active<4 or active>=9:native.controller_map_gc(ctypes.byref(raw,active*12),rng.randrange(0x2000),0,0,0,0,0,0)
  else:native.controller_map_classic(ctypes.byref(raw,active*12),ctypes.c_uint(rng.getrandbits(32)),index%3,128,128,128,128)
  u.mem_write(samples,b''.join(pad_be(raw.raw[i:i+12]) for i in range(0,156,12)));native.controller_focus_update(nf,raw);call('controller_focus_update',focus,samples)
  assert bytes(u.mem_read(focus,176))==focus_be(nf.raw)
  ng=ctypes.c_uint16();expected=native.controller_session(nf,ctypes.byref(ng));actual=call('controller_session',focus,guest)
  assert expected==actual and int.from_bytes(u.mem_read(guest,2),'big')==ng.value
  checked+=1
print(f'PASS {checked} actual linked PPC/native controller axis/mapping/focus/session comparisons; hardware not tested')
