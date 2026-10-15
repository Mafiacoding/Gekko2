#!/usr/bin/env python3
"""Mix observed BIOS blocks with guarded static game AOT. No private bytes bundled.

Seeds must be actual executable entry points or independently observed PCs.
Static selection weights are priorities, never claimed execution frequencies.
"""
import argparse,json,struct,collections
from pathlib import Path
from types import SimpleNamespace
import ee_aot_generate as aot

p=argparse.ArgumentParser();p.add_argument('--observed',required=True)
p.add_argument('--elf',required=True);p.add_argument('--pc',action='append',default=[])
p.add_argument('--output',required=True);args=p.parse_args()
segments=aot.load_segments(SimpleNamespace(elf=args.elf,image=None))
data=Path(args.elf).read_bytes();entry=struct.unpack_from('<I',data,24)[0]
seeds=[aot.number(x) for x in args.pc]+[entry]
pending=collections.deque(seeds);visited=set();game=[]
while pending and len(game)<256 and len(visited)<65536:
 pc=pending.popleft()
 if pc in visited or pc&3:continue
 visited.add(pc)
 values=[]
 for addr,blob in segments:
  if addr<=pc<addr+len(blob):
   n=min(32,(addr+len(blob)-pc)//4)
   values=list(struct.unpack_from('<'+str(n)+'I',blob,pc-addr));break
 if not values:continue
 words=aot.supported_prefix(pc,values,32)
 if words:
  game.append({'pc':hex(pc),'words':[hex(w) for w in words],
               'selection_weight':1000000-len(game),'provenance':'static executable decode'})
 else:
  # Do not scan blindly beyond an unsupported instruction.
  continue
 terminal=False
 for i,w in enumerate(words):
  site=pc+i*4;op=w>>26;rs=w>>21&31;rt=w>>16&31;fn=w&63
  if aot.has_delay_slot(w):
   if op in (2,3):pending.append(((site+4)&0xf0000000)|((w&0x3ffffff)<<2))
   elif op!=0:pending.append((site+4+struct.unpack('<h',struct.pack('<H',w&65535))[0]*4)&0xffffffff)
   if op not in (2,) and not (op==0 and fn==8):pending.append(site+8)
   terminal=True;break
  result=aot.translate(w,site)
  if result and result[1]:pending.append(site+4);terminal=True;break
 if not terminal:pending.append(pc+len(words)*4)
observed=json.loads(Path(args.observed).read_text())
bios=[]
for b in sorted(observed['blocks'],key=lambda b:-int(b['hits'])):
 pc=aot.number(b['pc'])
 if aot.supported_prefix(pc,b['words'],32) and (pc&0x1fffffff)>=0x1fc00000:
  b=dict(b);b['selection_weight']=2000000-len(bios);b['provenance']='observed BIOS';bios.append(b)
  if len(bios)==256:break
assert bios and game
observed.update(scope='Observed BIOS plus static game executable blocks seeded by host/Wii observed PCs; private input',blocks=bios+game,static_seeds=[hex(x) for x in seeds])
Path(args.output).write_text(json.dumps(observed))
print(json.dumps({'observed_bios_blocks':len(bios),'static_game_blocks':len(game),'game_entry':hex(entry),'visited':len(visited)}))
