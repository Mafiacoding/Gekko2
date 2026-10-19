#!/usr/bin/env python3
"""Linked PPC: an IOP deadline armed during catch-up limits later EE windows.

EE/IOP instructions and scheduler execute the supplied ELF. Only the external
deadline query is injected to isolate renewal; SDK services use the usual
fixture models. This is not a cycle-accuracy or physical-Wii test.
"""
from pathlib import Path
fixture = Path(__file__).with_name('verify_event_scheduler_r1353.py').read_text()
exec(compile(fixture.split('ee=[ins')[0], 'event_fixture', 'exec'))

deadline_calls = []
inject = [False]
def deadline(uc, address, size, user):
    if not inject[0] or address != syms['iop_core_event_distance']:
        return
    slots = executed()
    ticks = int.from_bytes(u.mem_read(ist + io['sched_ticks'], 8), 'big')
    maximum = uc.reg_read(UC_PPC_REG_3)
    deadline_calls.append((slots, ticks, maximum))
    # Initially quiet; after the first catch-up an external one-tick deadline
    # remains armed. A real device can change its deadline while IOP runs.
    uc.reg_write(UC_PPC_REG_3, min(maximum, 1) if ticks >= 8 else maximum)
    uc.reg_write(UC_PPC_REG_PC, uc.reg_read(UC_PPC_REG_LR))
u.hook_add(UC_HOOK_CODE, deadline)

ee = [ins(9, 2, 2, 1)] * 32 + [ins(4, imm=-33), 0]
iop = [ins(9, 4, 4, 1), ins(4, imm=-2), 0]
for grant in [9, 17, 64, 65, 129]:
    start(ee, iop, 0)
    expected = run(grant)
    start(ee, iop, 1 << 25)
    deadline_calls.clear()
    inject[0] = True
    actual = run(grant)
    inject[0] = False
    assert actual == expected, ('full state', grant)
    assert deadline_calls[0][:2] == (0, 0)
    refreshed = [c for c in deadline_calls if c[1] >= 8 and c[0] < grant * 8]
    assert refreshed, ('IOP deadline was never refreshed', grant, deadline_calls)
    assert refreshed[0][:2] == (64, 8), deadline_calls
    for before, after in zip(refreshed, refreshed[1:]):
        assert after[0] - before[0] <= 8, ('deadline overshoot', before, after)
        assert after[0] == after[1] * 8, ('IOP catch-up order', after)
    print('PASS renewed IOP deadline, exact EE/IOP/timer state, grant', grant)

# Completion must remove the callback; the legacy path must not query it.
deadline_calls.clear()
start(ee, iop, 0)
inject[0] = True
run(17)
inject[0] = False
assert not deadline_calls, deadline_calls
print('PASS legacy fallback independent of the event-window limiter')
