# R1312 unreleased EE JIT reachability audit

R1312 adds a structural source audit for the independently maintained EE
single-instruction front gate and the PPC translator. It is a development
milestone after the R1307 checkpoint, not a release/checkpoint and not a claim
that the full dynarec is complete.

## Change

`tools/audit-ee-jit-coverage.py` now extracts the actual C function bodies with
brace-depth parsing and removes comments before testing dispatch tokens. This
prevents historical comments elsewhere in either source file from satisfying a
coverage check accidentally.

The audit requires front + backend dispatch for SPECIAL, REGIMM, COP0, COP1,
COP2 and MMI. It pins the R1281 one-instruction COP0 contract to MFC0/MTC0,
checks all twelve R1310 integer-trap selectors remain reachable, checks the six
R1311 signed-overflow forms remain reachable, and requires the native trap and
overflow exception markers to remain in the PPC translator.

## Why this matters

R922 found a real class of failure where valid PPC emitters existed but the
GEKKO pre-filter never admitted their opcodes. Such code can look complete in a
backend-only review while real Wii execution still falls back to the scalar
interpreter. R1312 makes that class of regression machine-checkable.

This is a static reachability audit only. It does not replace linked-PPC
semantic/oracle tests, exception-state tests, Wii cross-builds, or physical Wii
validation. The next implementation work remains broader instruction/control
coverage, then wider residency/allocation and generation-guarded block linking.
