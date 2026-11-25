#!/usr/bin/env python3
"""Summarize private GAME_TRACE logs locally; no uploads or guessed gameplay status."""
import argparse,json,re
from collections import Counter
from pathlib import Path

def analyze(lines):
 counts=Counter();waiting={};overwritten=0;cpu={};unknown=0
 for line in lines:
  fields=dict(re.findall(r'(\w+)=([^\s]+)',line))
  if line.startswith('GAME_TRACE '):overwritten+=int(fields.get('overwritten','0'))
  elif line.startswith('GAME_EVENT '):
   kind=fields.get('kind','?');counts[kind]+=1
   if kind=='SIGNAL_WAIT':waiting[fields.get('a','?')]=fields
   elif kind=='SIGNAL_RESUME':waiting.pop(fields.get('a','?'),None)
  elif line.startswith('GAME_CPU '):cpu=fields
  elif line.startswith('GAME_UNKNOWN '):unknown+=1
 return dict(events=dict(counts),overwritten_events=overwritten,unmatched_signal_waits=list(waiting.values()),
             pinned_unknown_pairs=unknown,last_cpu=cpu,
             interpretation='Unmatched waits are observations, not proof of deadlock or its cause. Overflow can omit transitions; no gameplay detection.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',nargs='?',type=Path);p.add_argument('--self-test',action='store_true');a=p.parse_args()
 if a.self_test:
  x=analyze(['GAME_EVENT seq=1 kind=SIGNAL_WAIT a=00000000','GAME_EVENT seq=2 kind=CSR_ACK a=1','GAME_EVENT seq=3 kind=SIGNAL_RESUME a=00000000'])
  assert not x['unmatched_signal_waits'] and x['events']['CSR_ACK']==1
  y=analyze(['GAME_TRACE overwritten=9','GAME_EVENT seq=10 kind=SIGNAL_WAIT a=00000001'])
  assert len(y['unmatched_signal_waits'])==1 and y['overwritten_events']==9
  print('PASS trace parser matched/unmatched waits, CSR distinction and overflow')
 elif a.log:print(json.dumps(analyze(a.log.read_text(errors='replace').splitlines()),indent=2))
 else:p.error('provide a local log or --self-test')
