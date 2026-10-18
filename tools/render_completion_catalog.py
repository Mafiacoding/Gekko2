#!/usr/bin/env python3
"""Validate and render the tracked completion list without claiming conformance."""
from pathlib import Path
import argparse,collections,json
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args()
data=json.loads((root/'docs/COMPLETION-CATALOG.json').read_text());items=data['items'];seen=set()
lines=['# Gekko2 – Gesamtinventar und Umsetzungsstand','',
 'Basis BETA12.28 / `7681af2`. Diese Liste führt bestätigte Lücken, Teilpfade,',
 'absichtliche skalare Sicherheitsgrenzen und fehlende Hardwarebelege getrennt.',
 'Sie ist kein Beweis vollständiger PS2-Spezifikationsabdeckung. Alle erfassten',
 'Laufzeitquellen und rohe Reviewtreffer stehen im maschinenlesbaren Audit.',
 'Kommentar-Treffer können historische Fixes oder zulässige Rückfälle bezeichnen.',
 'Native Akzeptanz, tatsächliche PPC-Ausführung und Wii-Messung sind getrennte Gates.','',
 'Reproduktion: `python3 tools/audit_completion.py --output /tmp/completion.json`.',
 'Vorhandene MMI-, VU0-Macro- und VU-Upper-Audits zusätzlich ausführen.',
 'Öffentliche AOT-Tabellen sind absichtlich leer; das bedeutet keine fehlende ISA.','',
 '| Status | Anzahl |','|---|---:|']
for name,count in sorted(collections.Counter(x['status'] for x in items).items()):lines.append(f'| {name} | {count} |')
lines+=['','| ID | Befund / Aufgabe | Status | Code / Abnahme |','|---|---|---|---|']
for row in items:
 assert row['id'] not in seen;seen.add(row['id'])
 path=root/row['source'];assert path.is_file(),row['source']
 assert row['status'] in ('offen','teilweise','skalar','skalar_absicht','vorhanden_pruefen','hardware_nachweis','optional_abwesend','implementiert_getestet')
 if row['status']=='implementiert_getestet':assert row['tests'],row['id']
 evidence=f"[{row['source']}](../{row['source']})";gate=row['gate']
 lines.append(f"| {row['id']} | {row['title']} | {row['status']} | {evidence}; {gate} |")
lines+=['','## Arbeitsregeln','',
 '- Bei jedem Batch IDs, Codeänderung, tatsächlich ausgeführte Tests und Restgrenzen fortschreiben.',
 '- Ein implementierter Teilpfad schließt die übrigen Teilziele seines Moduls nicht automatisch.',
 '- Bewährte Interpreter-/Software-GS-Oracles, RAM-/Mapping-/IRQ-/Budget-Gates und EABI erhalten.',
 '- Als Nächstes: konkrete legale EE-Lücken, Audio-Datapaths und MCSERV-Erweiterungen.',
 '- Der unabhängige nächste Hardware-Schritt bleibt BIOS/Crash/Tekken im identischen Gastzustand.',
 '- Keine Nutzerdaten/BIOS/Discs/Karten/Profile in Tests oder öffentlichen Berichten.','']
text='\n'.join(lines);dest=root/'docs/COMPLETION-CATALOG.md'
if a.check:assert dest.read_text()==text,'catalog markdown is stale'
else:dest.write_text(text)
print('Validated catalog entries:',len(items))
