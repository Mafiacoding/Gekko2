"""Warm linear EE successor sequence; whole CPU path count and state digest."""
from pathlib import Path
text=Path(__file__).with_name('verify_ee_event_chain_r1369.py').read_text().split('checks=0;costs=')[0]
exec(compile(text,'event_chain_fixture','exec'))
warm_event_thunk=True
result,cost,selected=run(2,128,0,0)
print(json.dumps({'ppc_instructions':cost,'state_sha256':hashlib.sha256(result).hexdigest(),'selected_successors':selected,'retired':128}))
