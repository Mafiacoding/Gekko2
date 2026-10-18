#!/usr/bin/env python3
import contextlib,io,types
from unittest.mock import patch
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
  sender.settimeout(2)
  sender.sendto(b'bad',('127.0.0.1',port));sender.sendto(wire,('127.0.0.1',port));sender.sendto(wire,('127.0.0.1',port))
  expected=b'GK2A'+m.struct.pack('>QQ',r['boot'],r['seq'])
  assert sender.recvfrom(64)[0]==expected and sender.recvfrom(64)[0]==expected
  probe=bytearray(wire);m.struct.pack_into('>Q',probe,128+8*m.FIELDS.index('event'),6)
  m.struct.pack_into('>Q',probe,128+8*m.FIELDS.index('seq'),1)
  sender.sendto(probe,('127.0.0.1',port));assert sender.recvfrom(64)[0]==b'GK2A'+m.struct.pack('>QQ',r['boot'],1)
 stdout,stderr=proc.communicate(timeout=8);assert proc.returncode==0,(stdout,stderr)
 archives=list((temp/'output').glob('*.zip'));assert len(archives)==1
 with zipfile.ZipFile(archives[0]) as z:
  assert z.testzip() is None
  report=json.loads(z.read('summary.json'));assert report['packets']==2 and report['duplicates']==1 and report['invalid_packets']==1
  assert report['test_packets']==1 and len(report['groups'])==1
  assert report['receiver']['datagrams']==4 and report['receiver']['acknowledgments_sent']==3
 # Setup changes only telemetry.cfg; no CPU/renderer preferences.
 sd=temp/'SD';sd.mkdir();(sd/'pcsx2').mkdir();keep=sd/'pcsx2/optimization.cfg';keep.write_text('unchanged')
 setup=subprocess.run([sys.executable,str(root/'tools/windows-telemetry/receiver.py'),'--setup'],input=f'192.168.1.2\n{sd}\n',text=True,capture_output=True)
 assert setup.returncode==0,setup.stderr
 assert (sd/'pcsx2/telemetry.cfg').read_text()=='GEKKO2_TELEMETRY 1 192.168.1.2 26890\n' and keep.read_text()=='unchanged'
 # Empty capture now includes exact receiver diagnostics instead of just zero groups.
 empty=temp/'empty'
 run=subprocess.run([sys.executable,str(root/'tools/windows-telemetry/receiver.py'),'--listen','127.0.0.1','--port',str(port),'--seconds','0.1','--wait-seconds','0.1','--output',str(empty)],capture_output=True,text=True,timeout=5)
 assert run.returncode==2
 no_data=json.loads(next(empty.glob('*-summary.json')).read_text())
 assert no_data['receiver']['port']==port and no_data['receiver']['datagrams']==0 and no_data['receiver']['stop_reason']=='no_valid_packets'
 # Windows UDP can report ICMP reset when a Wii socket closes. Preserve report.
 class ResetSocket:
  def __init__(self):self.calls=0
  def bind(self,address):pass
  def settimeout(self,value):pass
  def close(self):pass
  def recvfrom(self,size):
   self.calls+=1
   if self.calls==1:raise ConnectionResetError(10054,'synthetic Windows UDP reset')
   raise socket.timeout()
 reset_output=temp/'reset'
 with patch.object(m.socket,'socket',return_value=ResetSocket()),contextlib.redirect_stdout(io.StringIO()):
  rc=m.collect(types.SimpleNamespace(output=reset_output,wii_ip=None,listen='127.0.0.1',port=26890,seconds=0.01,wait_seconds=0.01))
 assert rc==2
 reset_report=json.loads(next(reset_output.glob('*-summary.json')).read_text())
 assert reset_report['receiver']['receive_errors']==1 and reset_report['receiver']['last_receive_error']==10054
 assert len(list(reset_output.glob('*.zip')))==1
 print('PASS Windows-style UDP reset preserves capture/report')
 print('PASS menu test probe/ACK, no fake CPU scope, empty-receiver diagnostics and received/ack counters')
 print('PASS C/Python wire interoperability, malformed packets, duplicate/reordered/lost packets, epoch/mask grouping, cumulative scope totals, UDP receive/report ZIP and SD-only configuration')
