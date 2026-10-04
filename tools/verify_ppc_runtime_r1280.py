"""Test final Wii ELF Remote mapping, old-libogc idle timer, FPS and budget helpers.
Bluetooth calls are mocked: this is not a physical connection test.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_remote_r1279.py').read_text(),'verify_ppc_remote_r1279.py','exec'))
from unicorn import UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_LR,UC_PPC_REG_PC
import random
# Execute the bundled old libogc timer with a synthetic connected Remote.
# Its disconnect callback is intercepted; no real Bluetooth device is present.
def put32(addr,value):u.mem_write(addr,struct.pack('>I',value))
cb=syms['__wpdcb'];fake=0x81701000
u.mem_write(cb,bytes(29300));u.mem_write(fake,bytes(1024))
put32(cb,fake);put32(fake+48,16);put32(syms['__wpads_active'],1)
disconnected=[]
def intercept(uc,address,size,data):
 if address==syms['wiiuse_disconnect']:
  disconnected.append(uc.reg_read(UC_PPC_REG_3));uc.reg_write(UC_PPC_REG_PC,uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE,intercept);u.ctl_remove_cache(0x80000000,0x81800000)
call('WPAD_SetIdleTimeout',0);call('__wpad_timeouthandler')
assert disconnected==[fake],disconnected
put32(cb+28,0);disconnected.clear()
call('WPAD_SetIdleTimeout',0xffffffff)
assert bytes(u.mem_read(syms['__wpad_idletimeout'],4))==b'\xff'*4
for n in range(601):call('__wpad_timeouthandler')
assert not disconnected and bytes(u.mem_read(cb+28,4))==struct.pack('>I',601)
for count,ms,expected in [(3,5000,600),(1,12500,80),(60,1000,60000),(0,0,0),((1<<64)-1,1,0xffffffff)]:
 assert call('frontend_rate_milli',count>>32,count&0xffffffff,ms>>32,ms&0xffffffff)==expected
for now,last,events,prev,expected in [(499,0,10,10,0),(500,0,10,10,1),(1,0,11,10,1),(0,1,10,10,1),((1<<40)+499,1<<40,10,10,0)]:
 args=[]
 for v in [now,last,events,prev]:args += [v>>32,v&0xffffffff]
 assert call('frontend_present_due',*args)==expected
random.seed(1280)
for n in range(1000):
 previous=random.randint(32,50000);ms=random.randint(0,10000);target=random.choice([20,50])
 expected=max(32,min(50000,previous*2,previous*target//max(1,ms)))
 assert call('frontend_next_budget',previous,ms,target)==expected
print('PASS actual bundled libogc zero-timeout disconnect reproduced; UINT_MAX remains connected for 601 synthetic idle ticks')
print('PASS actual Wii ELF FPS rates, 64-bit display cadence and 1000 adaptive CPU-budget cases; no hardware FPS claim')
