"""Actual PPC boot-policy checks plus integration location and log-mode audit."""
from pathlib import Path
exec(compile(Path(__file__).with_name('verify_ppc_gs_memory.py').read_text(),'verify_ppc_gs_memory.py','exec'))
for due in [0,1]:
 for active in [0,1]:
  for first in [0,1]:
   for hud in [0,1]:
    for requested in [0,1]:
     assert call('frontend_probe_first_image',due,active,first)==int(due and active and not first)
     assert call('frontend_allow_gx_primitives',requested,first)==int(requested and first)
     if 'frontend_allow_gx_output' in syms:
      for psm in range(32):assert call('frontend_allow_gx_output',requested,first,psm)==int(requested and first and psm in [0,1])
source=Path(__file__).resolve().parents[1]/'source/main.c';s=source.read_text()
probe=s.index('if(frontend_probe_first_image(')
output=s.index('if (present_due && display_active && !show_hud)',probe)
assert probe<output
assert 'g_gx_present = 0' in s
if 'R1296-software.log' in s:
 assert 'g_gx_primitives' not in s
 assert 'if(down&(PAD_BUTTON_RIGHT|PAD_BUTTON_LEFT))' in s
 assert 'frontend_allow_gx_primitives(g_gx_present,g_first_picture)' in s
if 'R1294-software.log' in s or 'R1295-software.log' in s:assert 'if(frontend_allow_gx_output(g_gx_present,g_first_picture,' in s
assert 'gs_gx_set_render_enabled(g_gx_present)' not in s
if 'R1296-software.log' in s:
 modes=['software','gx-render'];revision='R1296'
else:
 modes=['software','gx-output','gx-render'];revision='R1295' if 'R1295-software.log' in s else 'R1294' if 'R1294-software.log' in s else 'R1293' if 'R1293-software.log' in s else 'R1292' if 'R1292-software.log' in s else 'R1291'
for mode in modes:assert revision+'-'+mode+'.log' in s
assert 'primitives_active=%d' in s and 'sync_errors=%lu' in s and 'hud=%d' in s
print('PASS 32 actual PPC boot policies; source integration keeps image probe outside HUD gate, gates GX drawing until the first image and preserves logs by mode; no Wii cold-boot proof')
