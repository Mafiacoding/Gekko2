#!/usr/bin/env python3
import copy,importlib.util,json,os,socket,subprocess,sys,tempfile,time,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('receiver',root/'tools/windows-telemetry/receiver.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as temp:
 temp=Path(temp);exe=temp/'protocol';packet=temp/'packet'
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-Iinclude','tests/test_runtime_telemetry.c','source/hw/runtime_telemetry.c','-o',str(exe)],cwd=root,check=True)
 subprocess.run([str(exe),str(packet)],check=True)
 wire=packet.read_bytes();r=m.decode(wire)
 assert r['boot']==0x123456789abcdef0 and r['active_mask']==0x787dff
 assert r['cpu']['ticks']==list(range(1,15)) and r['cpu']['samples']==99 and r['cpu']['overflows']==3
 assert r['cpu']['work'][13]==(1<<40)+13 and r['present']['ticks'][0]==100
 for bad in (wire[:-1],wire+b'x',b'BAD!'+wire[4:],wire[:4]+b'\2'+wire[5:],wire[:7]+b'\1'+wire[8:]):
  try:m.decode(bad)
  except ValueError:pass
  else:raise AssertionError('bad packet accepted')
 r['sender']='192.168.1.9';r['interval_ms']=5000;r['core_tb']=60750000
 newer=copy.deepcopy(r);newer['seq']=2;newer['cpu']['ticks'][0]+=100;newer['tx_dropped']=1
 result=m.summary([newer,r,r]);assert result['packets']==2 and result['duplicates']==1
 g=result['groups'][0];assert g['sequence_gaps']==1 and g['interval_ms']==10000
 assert g['scopes']['cpu']['total_tb']==sum(newer['cpu']['ticks']) # cumulative, no double counting
 assert g['scopes']['present']['total_tb']==sum(newer['present']['ticks'])
 different=copy.deepcopy(newer);different['seq']=3;different['active_mask']=0;different['epoch']=2
 assert len(m.summary([r,different])['groups'])==2
 assert any('interpreter' in w for w in m.summary([different])['groups'][0]['warnings'])
 # Actual UDP loopback sender -> CLI -> JSONL/summary -> single ZIP.
 sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',0));port=sock.getsockname()[1];sock.close()
 proc=subprocess.Popen([sys.executable,str(root/'tools/windows-telemetry/receiver.py'),'--listen','127.0.0.1','--port',str(port),'--seconds','0.3','--wait-seconds','2','--output',str(temp/'output')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 assert 'Warte auf Wii' in proc.stdout.readline()
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sender:
  sender.sendto(b'bad',('127.0.0.1',port));sender.sendto(wire,('127.0.0.1',port));sender.sendto(wire,('127.0.0.1',port))
 stdout,stderr=proc.communicate(timeout=8);assert proc.returncode==0,(stdout,stderr)
 archives=list((temp/'output').glob('*.zip'));assert len(archives)==1
 with zipfile.ZipFile(archives[0]) as z:
  assert z.testzip() is None
  report=json.loads(z.read('summary.json'));assert report['packets']==1 and report['duplicates']==1 and report['invalid_packets']==1
 # Setup changes only telemetry.cfg; no CPU/renderer preferences.
 sd=temp/'SD';sd.mkdir();(sd/'pcsx2').mkdir();keep=sd/'pcsx2/optimization.cfg';keep.write_text('unchanged')
 setup=subprocess.run([sys.executable,str(root/'tools/windows-telemetry/receiver.py'),'--setup'],input=f'192.168.1.2\n{sd}\n',text=True,capture_output=True)
 assert setup.returncode==0,setup.stderr
 assert (sd/'pcsx2/telemetry.cfg').read_text()=='GEKKO2_TELEMETRY 1 192.168.1.2 26890\n' and keep.read_text()=='unchanged'
 print('PASS C/Python wire interoperability, malformed packets, duplicate/reordered/lost packets, epoch/mask grouping, cumulative scope totals, UDP receive/report ZIP and SD-only configuration')
