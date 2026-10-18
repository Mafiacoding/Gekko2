#!/usr/bin/env python3
"""Gekko2 local UDP receiver. Python 3.9+, standard library, no remote control."""
import argparse,datetime,ipaddress,json,os,re,socket,struct,time,zipfile
from pathlib import Path
FIELDS='boot seq session ms active_mask gx experiments interval_ms presents guest_vblanks ee_retired core_tb present_tb log_tb gx_timeouts ee_block_retired ee_budget_misses ee_way_scans tx_dropped tx_errors tx_sent event epoch'.split()
CATEGORIES='EE_dispatch_scalar IOP scheduler idle compile GS_raster GX_pack GX_readback GX_wait present IPU EE_native_attempt VU0 VU1'.split()
WIRE=struct.Struct('>4sBBH80s40s'+'Q'*(len(FIELDS)+88))
assert WIRE.size==1016
TB_HZ=60750000

def decode(packet):
 if len(packet)!=WIRE.size:raise ValueError('packet length')
 raw=WIRE.unpack(packet)
 if raw[:4]!=(b'GK2T',1,0,0):raise ValueError('schema')
 build=raw[4].split(b'\0',1)[0].decode('ascii')
 commit=raw[5].rstrip(b'\0').decode('ascii')
 if not re.fullmatch(r'[A-Za-z0-9_.-]{1,79}',build) or not re.fullmatch(r'[0-9a-f]{40}|unknown',commit):raise ValueError('build')
 r=dict(zip(FIELDS,raw[6:6+len(FIELDS)]));r.update(build=build,commit=commit)
 if r['gx']>1 or r['active_mask']>0xffffffff or r['experiments']>7 or r['event'] not in (0,2,3,4):raise ValueError('options/event')
 offset=6+len(FIELDS)
 for scope in ('cpu','present'):
  values=raw[offset:offset+44];offset+=44
  r[scope]=dict(ticks=list(values[:14]),entries=list(values[14:28]),work=list(values[28:42]),samples=values[42],overflows=values[43])
 return r

def summary(records):
 # Deduplicate by sender/boot/sequence; late packets never replace newer totals.
 unique={}
 for r in records:unique.setdefault((r['sender'],r['boot'],r['seq']),r)
 groups={};streams={}
 for r in unique.values():
  stream=(r['sender'],r['boot']);streams.setdefault(stream,[]).append(r['seq'])
  key=(r['sender'],r['boot'],r['session'],r['epoch'],r['build'],r['commit'],r['active_mask'],r['gx'],r['experiments'])
  g=groups.setdefault(key,dict(latest=r,rows=0,core_tb=0,present_tb=0,log_tb=0,interval_ms=0,ee_retired=0,presents=0,guest_vblanks=0))
  g['rows']+=1
  if r['seq']>g['latest']['seq']:g['latest']=r
  for field in ('core_tb','present_tb','log_tb','interval_ms','ee_retired','presents','guest_vblanks'):g[field]+=r[field]
 output=[]
 for key,g in groups.items():
  last=g.pop('latest');g.update(build=last['build'],commit=last['commit'],session=last['session'],epoch=last['epoch'],active_mask=f"{last['active_mask']:08x}",gx=last['gx'],experiments=last['experiments'],scopes={})
  warnings=[]
  if not(last['active_mask']&7):warnings.append('EE/IOP/VU JIT disabled: interpreter configuration')
  if last['gx_timeouts']:warnings.append('GX timeout observed; SD fault log still needed for command-level diagnosis')
  if last['tx_dropped'] or last['tx_errors']:warnings.append('Sender reported dropped packets or send errors')
  seqs=streams[(last['sender'],last['boot'])];missing=max(seqs)-min(seqs)+1-len(set(seqs))
  g.update(sequence_gaps=missing,tx_dropped=last['tx_dropped'],tx_errors=last['tx_errors'],gx_timeouts=last['gx_timeouts'],ee_block_retired=last['ee_block_retired'],ee_budget_misses=last['ee_budget_misses'])
  if missing:warnings.append('Packet gaps: interval wall-time totals are incomplete')
  for scope in ('cpu','present'):
   data=last[scope];total=sum(data['ticks'])
   g['scopes'][scope]=dict(total_tb=total,ms=1000*total/TB_HZ,samples=data['samples'],overflows=data['overflows'],categories=[dict(category=c,ticks=t,entries=e,work=w,share_percent=100*t/total if total else 0) for c,t,e,w in zip(CATEGORIES,data['ticks'],data['entries'],data['work'])])
  g['warnings']=warnings;output.append(g)
 return dict(schema=1,packets=len(unique),duplicates=len(records)-len(unique),groups=output,
 notes=['CPU sampling and exact presentation have separate denominators.',
 'Scope counters are cumulative within each epoch; missing UDP packets may hide the final tail.',
 'Native attempt entries do not prove native retirement; work units differ by category.',
 'Each active configuration/epoch is separate. Different scenes are not a valid speed comparison.',
 'No Wii gameplay, graphical correctness or FPS improvement is inferred.'])

def private_ip(text):
 ip=ipaddress.IPv4Address(text)
 if not any(ip in ipaddress.IPv4Network(n) for n in ('10.0.0.0/8','172.16.0.0/12','192.168.0.0/16')):raise ValueError('Use the Windows PC LAN IPv4 address (10.x, 172.16-31.x or 192.168.x).')
 return str(ip)

def setup():
 print('Gekko2: einmalige SD-Konfiguration. Keine Emulator-Option wird geaendert.')
 try:print('Moegliche Rechner-Adressen:',', '.join(socket.gethostbyname_ex(socket.gethostname())[2]))
 except OSError:pass
 ip=private_ip(input('LAN-IPv4 dieses Windows-Rechners: ').strip())
 root=Path(input('SD-Laufwerk/Pfad (z.B. E:\\): ').strip().strip('"'))
 if not root.is_dir():raise ValueError('SD-Pfad existiert nicht')
 directory=root/'pcsx2';directory.mkdir(exist_ok=True)
 target=directory/'telemetry.cfg';temp=directory/'telemetry.cfg.tmp'
 temp.write_text(f'GEKKO2_TELEMETRY 1 {ip} 26890\n',encoding='ascii');os.replace(temp,target)
 print('Gespeichert:',target,'\nDanach Gekko2 komplett aus HBC neu starten. UDP-Port 26890 im privaten Netzwerk erlauben.')

def collect(args):
 directory=args.output;directory.mkdir(parents=True,exist_ok=True)
 stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
 rawpath=directory/f'gekko2-network-{stamp}.jsonl';records=[];invalid=0;source=args.wii_ip
 sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
 sock.bind((args.listen,args.port));sock.settimeout(0.5)
 print(f'Warte auf Wii UDP {args.port}; nach dem ersten Paket {args.seconds:g} Sekunden. Strg+C beendet frueher.',flush=True)
 first=None;waiting=time.monotonic()
 try:
  with rawpath.open('x',encoding='utf-8') as log:
   while len(records)<10000:
    now=time.monotonic()
    if first is not None and now-first>=args.seconds:break
    if first is None and now-waiting>=args.wait_seconds:break
    try:packet,address=sock.recvfrom(WIRE.size+1)
    except socket.timeout:continue
    if source and address[0]!=source:continue
    try:r=decode(packet)
    except (ValueError,UnicodeError,struct.error):invalid+=1;continue
    if source is None:source=address[0];print('Wii erkannt:',source,flush=True)
    if first is None:first=time.monotonic()
    r['sender']=source;records.append(r);log.write(json.dumps(r,separators=(',',':'))+'\n');log.flush()
    print(f"Session {r['session']}  Paket {r['seq']}  CPU={r['active_mask']:08x} GX={r['gx']}  GX-Timeouts={r['gx_timeouts']}",flush=True)
 except KeyboardInterrupt:pass
 finally:sock.close()
 report=summary(records);report['invalid_packets']=invalid
 reportpath=directory/f'gekko2-network-{stamp}-summary.json';reportpath.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 archive=directory/f'gekko2-network-{stamp}.zip'
 with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
  z.write(rawpath,'telemetry.jsonl');z.write(reportpath,'summary.json')
 print('Fertig. Nur diese Datei bereitstellen:',archive,flush=True)
 if not records:print('Keine Daten empfangen: Sender-IP, Firewall/privates Netzwerk, SD-Konfiguration und Neustart aus HBC pruefen.')
 return 0 if records else 2

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--setup',action='store_true');p.add_argument('--listen',default='0.0.0.0');p.add_argument('--port',type=int,default=26890);p.add_argument('--wii-ip');p.add_argument('--seconds',type=float,default=600);p.add_argument('--wait-seconds',type=float,default=120);p.add_argument('--output',type=Path,default=Path('reports'));a=p.parse_args()
 if not 1<=a.port<=65535 or not 0<a.seconds<=86400 or not 0<a.wait_seconds<=86400:p.error('Invalid port or duration')
 try:
  if a.setup:setup();return 0
  return collect(a)
 except (OSError,ValueError) as e:p.exit(1,str(e)+'\n')
if __name__=='__main__':raise SystemExit(main())
