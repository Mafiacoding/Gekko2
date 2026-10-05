#!/usr/bin/env python3
from pathlib import Path
p=Path('tools/r1318_apply.py')
s=p.read_text()
old="""once(\n    'source/core/recompiler/ee_jit.c',\n    '#endif /* GEKKO */',\n    '#endif /* GEKKO || EE_JIT_CACHE_TEST */',\n)\n"""
new="""once(\n    'source/core/recompiler/ee_jit.c',\n    '}\\n\\n#endif /* GEKKO */\\n\\nstatic int ee_jit_resolve_and_execute',\n    '}\\n\\n#endif /* GEKKO || EE_JIT_CACHE_TEST */\\n\\nstatic int ee_jit_resolve_and_execute',\n)\n"""
assert s.count(old)==1, s.count(old)
p.write_text(s.replace(old,new,1))
