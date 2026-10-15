"""Linked PPC exact best-way interval, mutation invalidation and lookup cost.
 * Fake code owners are never executed; separate budget-edge suite tests guest execution. */"""
from pathlib import Path
exec(compile(Path(__file__).with_name('ee_ppc_fixture_internal.py').read_text(),'fixture','exec'))
call('ee_jit_reset_stats_for_test')
mask=syms['gekko2_optimization_mask'];word(mask,(int.from_bytes(u.mem_read(mask,4),'big')|8192)&~(1<<17))
assert call('ee_jit_get_cache_entries')==16384
pc=0x80200000;key=((pc>>2)^(pc>>12)^(pc>>22))*0x9e3779b1&0xffffffff;index=(key^(key>>16))&4095
slots=[syms['precise_cache']+(index+w*4096)*precise_slot_size for w in range(2)]
def publish(slot,count):
 word(slot,pc);word(slot+precise_count_offset,count);word(slot+precise_fn_offset,0x80000100)
 call('ee_precise_publish_dispatch',slot)
publish(slots[0],8);publish(slots[1],16)
active[0]=True;total[0]=0
for repeat in range(100):
 for budget in range(8,65):assert call('ee_precise_find',pc,0,budget)==slots[budget>=16],budget
active[0]=False
def stat(n):return (call('ee_jit_get_budget_cache_stat',n)<<32)|u.reg_read(UC_PPC_REG_4)
print('BUDGET_RANGE_COST',json.dumps({'calls':5700,'PPC_instructions':total[0],'interval_hits':stat(6),'way_scans':stat(7)}))
if '1374' in Path(a.elf).name:assert stat(6)>5000 and stat(7)==200
# Changing a best-way owner invalidates its hint, even for the same PC.
publish(slots[0],12)
for budget in range(1,25):
 expected=0 if budget<12 else slots[0] if budget<16 else slots[1]
 assert call('ee_precise_find',pc,0,budget)==expected,budget
# Removing the larger variant must widen the short variant's interval.
word(slots[1]+precise_fn_offset,0);call('ee_precise_publish_dispatch',slots[1])
for budget in range(12,65):assert call('ee_precise_find',pc,0,budget)==slots[0]
print('PASS interval chooses exact largest fitting way; low budgets, owner mutation and variant removal invalidate hints')
