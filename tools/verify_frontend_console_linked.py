#!/usr/bin/env python3
"""Execute the actual linked Wii console gates with synthetic SDK writers.
No IOS, GPU or physical Wii execution; backend callbacks are PPC test stubs.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text().split('checks=0')[0], 'loader', 'exec'))
# Separate mapped code avoids overwriting any linked image or platform service.
stub=0x92000000
u.mem_map(stub,4096)
# Return original callback's error or partial count, proving delegation rather
# than merely reporting an expected length. No printf / framebuffer is mocked.
u.mem_write(stub,struct.pack('>4I',0x3860fff9,0x4e800020,0x38600002,0x4e800020))
def word(at,n):u.mem_write(at,struct.pack('>I',n))
checks=0
for enabled in [0,1,0,1]:
 word(syms['g_console_visible'],enabled)
 for index,name in enumerate(['screen_stdout','screen_stderr']):
  for callback,result in [(stub,0xfffffff9),(stub+8,2),(0,0)]:
   word(syms['g_screen_writers']+index*4,callback)
   for length in [0,1,29,1024]:
    # Invalid payload address is deliberate: a disabled gate must not read it.
    word(syms['g_loading_valid'],1)
    got=call(name,0,0,0xdead0000,length)
    assert int.from_bytes(u.mem_read(syms['g_loading_valid'],4),'big')==(0 if enabled and callback else 1)
    assert got==(result if enabled and callback else length),(enabled,name,callback,length,got)
    checks+=1
print('PASS',checks,'linked PPC console routing checks: OFF discards, ON delegates, errors/partial writes preserved; visible console invalidates loading cache')
