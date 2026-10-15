from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
u.reg_write(UC_PPC_REG_2,syms['__SDATA2_START__']+0x8000);u.reg_write(UC_PPC_REG_13,syms['__SDATA_START__']+0x8000)
# Synthetic encodings; no BIOS or game bytes are required.
ops=[]
for fn in list(range(32))+[0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f]:
 for fs,ft in [(0,4),(4,0),(0,0)]:
  for maskbits in [1,8,15]:
   ops.append((0x12<<26)|((16|maskbits)<<21)|(ft<<16)|(fs<<11)|(5<<6)|fn)
for rd in [0,4]:
 ops.append((0x12<<26)|(1<<21)|(5<<16)|(rd<<11)) # QMFC2
mask=syms['gekko2_optimization_mask'];checked=0
for index,iw in enumerate(ops):
 setup();word(mask,1|8);word(state+off['cop0']+12*4,0x70000000)
 # Nonzero, exact floats with different lane values and moderate integers.
 for reg in range(1,32):
  u.mem_write(state+reg*16,struct.pack('>QQ',0x12345600000000+reg,0x789abc00000000+reg))
  word(state+off['fpr']+reg*4,struct.unpack('>I',struct.pack('>f',(reg-16)*0.25))[0])
  for lane in range(4):word(state+off['vu0_vf']+reg*16+lane*4,struct.unpack('>I',struct.pack('>f',(reg+lane-16)*0.25))[0])
 initial=bytes(u.mem_read(state,size))
 ok=call('ee_jit_try_execute_one_at',state,base,iw)
 if not ok:continue
 actual=bytes(u.mem_read(state,size))
 u.mem_write(state,initial);word(mask,0);u.mem_write(ram+base,struct.pack('<I',iw));call('ee_core_step_n',1)
 expected=bytes(u.mem_read(state,size))
 # Compare arithmetic data, omit retirement/control fields.
 spans=[(0,512),(off['fpr'],128),(off['vu0_vf'],512)]
 for start,n in spans:
  if actual[start:start+n]!=expected[start:start+n]:
   differences=[(k,actual[k:k+4].hex(),expected[k:k+4].hex()) for k in range(start,start+n,4) if actual[k:k+4]!=expected[k:k+4]]
   print('MISMATCH',hex(iw),differences[:12],flush=True);raise SystemExit(1)
 checked+=1
 if checked%100==0:print('checked',checked,flush=True)
print('PASS',checked,'synthetic VF00 coprocessor encodings',flush=True)
