# R1319 working notes — exact MMI precise-block admission

R1319 closes a reachability gap between the EE precise-block frontend and the existing PowerPC MMI backend. The single-instruction JIT intentionally keeps a broad `op == 0x1c` gate and lets `ppc_dynarec_translate_one()` exact-decode the MMI instruction. That policy is inappropriate for multi-instruction precise blocks: admitting an unknown MMI selector makes the whole block translation fail and silently forfeits native execution for otherwise valid neighbours.

`ee_block_policy.h` therefore now has an exact MMI admission predicate. It admits the legal primary MMI operations and the legal MMI0/MMI1/MMI2/MMI3 subgroup selectors already supported by the backend, while declining unknown selectors before block formation. PMFHL is restricted to its five implemented architectural modes. Unsupported encodings retain the scalar/interpreter fallback.

Verification is intentionally independent of stale coverage comments. `tools/audit_mmi_block_admission.py` derives the legal selector map from the checked-in PCSX2 R5900 opcode tables, exhausts all 64 primary funct values times all 32 `sa` values, and requires the block policy to match that legality map. It also checks the 103 canonical legal MMI encodings used by the existing R1315 coverage audit against both `ppc_dynarec_translate_one()` and `ppc_dynarec_translate_ee_resident_delay_block()`.

The focused R1319 gate requires:

- all 2,048 primary-funct/`sa` selector pairs to match the reference legality map;
- all 103 canonical legal MMI encodings to be admitted by the precise-block policy;
- all 103 canonical legal encodings to be accepted by both single-op and resident-block PPC translation;
- the existing internal MMI coverage and EE JIT reachability audits to remain green;
- the full host regression suite to pass before the change is promoted to the active dynarec branch.

This is a frontend reachability/correctness change, not a claim that every MMI instruction has been revalidated on physical Wii hardware. The audit compiles and inspects generated PPC paths on the host but does not execute those PPC buffers natively. Physical Wii stability and performance remain separate release gates.
