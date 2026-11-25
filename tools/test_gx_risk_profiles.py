#!/usr/bin/env python3
"""Isolated option availability and admission tests for four compile profiles."""
from pathlib import Path
import argparse,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path);a=p.parse_args()
backend=['-DGEKKO2_GX_NATIVE_BIOS_TRI','-DGEKKO2_GX_GENERIC_SHADER','-DGEKKO2_GX_EXTENDED_TRIANGLES']
profiles={'uncompiled':[], 'flag_without_backend':['-DGEKKO2_GX_RISK_MODE'],
          'risk':['-DGEKKO2_GX_RISK_MODE',*backend],
          'risk_chain':['-DGEKKO2_GX_RISK_MODE',*backend,'-DGEKKO2_EE_CHAIN_RESIDENCY']}
texts=[]
with tempfile.TemporaryDirectory() as d:
 stub=Path(d)/'mask.c';stub.write_text('#include <stdint.h>\nuint32_t gekko2_optimization_mask=0x12345678;\n')
 for name,flags in profiles.items():
  exe=Path(d)/name
  subprocess.run(['gcc','-O2','-Iinclude','-ffunction-sections','-fdata-sections',*flags,
   'tests/test_gx_risk_mode.c','source/core/recompiler/experimental_options.c',
   'source/hw/gs_gx_surface.c',str(stub),'-Wl,--gc-sections','-o',str(exe)],cwd=root,check=True)
  texts.append(name+': '+subprocess.check_output([str(exe)],text=True))
text=''.join(texts)
if a.output:a.output.write_text(text)
print(text,end='')
