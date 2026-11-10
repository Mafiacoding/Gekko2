#!/usr/bin/env python3
"""Execute all eight saved experiment combinations on the delivered PPC ELF.
Platform services are mocked by the common fixture, not Wii hardware results.
GS format correctness and GX risk admission have separate host suites.
"""
from pathlib import Path
source=Path(__file__).with_name('verify_chain_residency_linked.py').read_text()
exec(compile(source.split('checks=selected=0')[0],'verify_chain_residency_linked.py','exec'))
assert call('gekko2_experimental_available')&7==7
checks=successors=0
for budget in (17,128):
 reference,_=run(False,budget,0,0)
 for chain_mode in (0,1):
  for psm in (0,1):
   for risk in (0,1):
    result,n=run(True,budget,0,0,chain_mode=chain_mode,psm_mode=psm,risk_mode=risk)
    assert result==reference,(budget,chain_mode,psm,risk)
    assert [call('gekko2_experimental_enabled',i) for i in range(3)]==[chain_mode,psm,risk]
    if chain_mode:assert n>0
    else:assert n==0
    # Requests and reset values must leave the running session untouched.
    call('gekko2_experimental_request',0,1-chain_mode)
    call('gekko2_experimental_request',1,1-psm)
    call('gekko2_experimental_request',2,1-risk)
    assert [call('gekko2_experimental_enabled',i) for i in range(3)]==[chain_mode,psm,risk]
    checks+=1;successors+=n
assert successors
print(f'PASS {checks} linked PPC comparisons across all 8 chain/PSM/GX-risk modes; cold-boot isolation, fallback and successors {successors}')
