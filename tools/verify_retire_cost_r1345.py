from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'ee_ppc_fixture_internal.py','exec'))
extension_setup()
word(state+off['cop0']+11*4,0xffffffff)
word(state+off['cop0']+12*4,0)
active[0]=True;total[0]=0
for n in range(512):call('ee_core_block_commit',state)
active[0]=False
assert executed()==512
assert int.from_bytes(u.mem_read(state+off['cop0']+9*4,4),'big')==612
print('RETIRE_SIGNATURE',hashlib.sha256(bytes(u.mem_read(state,size))).hexdigest())
print('RETIRE_PPC_INSTRUCTIONS',total[0])
print('PASS 512 exact EE retirement/Count updates with live device state')
