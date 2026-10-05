#!/usr/bin/env python3
from pathlib import Path


def once(path, old, new):
    p = Path(path)
    s = p.read_text()
    assert s.count(old) == 1, (path, s.count(old), old[:120])
    p.write_text(s.replace(old, new, 1))


once(
    'include/core/recompiler/ee_jit.h',
    '''uint64_t ee_jit_get_compile_attempt_count(void);\nuint32_t ee_jit_get_cache_size(void);\nuint64_t ee_jit_get_pc_l0_hit_count(void);''',
    '''uint64_t ee_jit_get_compile_attempt_count(void);\nuint32_t ee_jit_get_cache_size(void);\n/* R1318: bounded single-op code-cache replacement diagnostics. */\nuint64_t ee_jit_get_eviction_count(void);\nuint64_t ee_jit_get_pc_l0_hit_count(void);''',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''static ee_jit_cache_slot_t g_cache[EE_JIT_CACHE_SLOTS];\nstatic uint32_t g_cache_count = 0;\nstatic uint64_t g_jit_executed = 0;''',
    '''static ee_jit_cache_slot_t g_cache[EE_JIT_CACHE_SLOTS];\nstatic uint32_t g_cache_count = 0;\n/* R1318: cache-full replacement is round-robin. A generated single-op body\n * is pinned while executing so a re-entrant helper can never evict it. */\nstatic uint32_t g_cache_evict_cursor = 0;\nstatic uint64_t g_cache_evictions = 0;\nstatic unsigned g_single_active = 0;\nstatic uint64_t g_jit_executed = 0;''',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''/* These three helpers are only reachable from the GEKKO branch of\n * ee_jit_try_execute_one() below (see that function's host-safety-gate\n * comment) - wrapped in #ifdef GEKKO here too so host-native builds\n * (which never call them) don't warn about unused static functions. */\n#ifdef GEKKO''',
    '''/* Opcode/cache helpers are used by GEKKO execution. R1318 also exposes\n * the ownership-only cache helpers to a host bookkeeping unit test; no PPC\n * generated body is executed by that test. */\n#if defined(GEKKO) || defined(EE_JIT_CACHE_TEST)''',
)

old_cache = '''/* Instruction-keyed owning cache. Once full, existing entries remain usable;\n * uncached encodings fall back to the interpreter without allocating code.\n * L0 remembers deterministic rejection with both PC and instruction tags.\n * Allocation/finalization failures are transient and are never remembered. */\nstatic ppc_block_fn ee_jit_cache_lookup(uint32_t instr)\n{\n    uint32_t h = (instr * 2654435761u) & (EE_JIT_CACHE_SLOTS - 1u);\n    for (uint32_t probe = 0; probe < EE_JIT_CACHE_SLOTS; probe++) {\n        uint32_t slot = (h + probe) & (EE_JIT_CACHE_SLOTS - 1u);\n        if (g_cache[slot].fn == NULL)\n            return NULL; /* empty slot reached along the probe chain: definitely not cached */\n        if (g_cache[slot].instr == instr)\n            return g_cache[slot].fn;\n    }\n    return NULL; /* cache full and not found */\n}\n\nstatic int ee_jit_cache_insert(uint32_t instr, ppc_block_fn fn)\n{\n    if (g_cache_count >= EE_JIT_CACHE_SLOTS)\n        return 0; /* No ownership transfer when full. */\n    uint32_t h = (instr * 2654435761u) & (EE_JIT_CACHE_SLOTS - 1u);\n    for (uint32_t probe = 0; probe < EE_JIT_CACHE_SLOTS; probe++) {\n        uint32_t slot = (h + probe) & (EE_JIT_CACHE_SLOTS - 1u);\n        if (g_cache[slot].fn == NULL) {\n            g_cache[slot].instr = instr;\n            g_cache[slot].fn = fn;\n            g_cache_count++;\n            return 1;\n        }\n    }\n    return 0;\n}\n'''
new_cache = '''/* Instruction-keyed owning cache. R1318 keeps it strictly bounded but no\n * longer turns a full cache into a permanent interpreter fallback. Once all\n * slots are occupied, replacement is round-robin. Because the table remains\n * full, replacing an occupied slot cannot break an open-addressing probe\n * chain: misses already scan all EE_JIT_CACHE_SLOTS entries. */\nstatic ppc_block_fn ee_jit_cache_lookup(uint32_t instr)\n{\n    uint32_t h = (instr * 2654435761u) & (EE_JIT_CACHE_SLOTS - 1u);\n    for (uint32_t probe = 0; probe < EE_JIT_CACHE_SLOTS; probe++) {\n        uint32_t slot = (h + probe) & (EE_JIT_CACHE_SLOTS - 1u);\n        if (g_cache[slot].fn == NULL)\n            return NULL;\n        if (g_cache[slot].instr == instr)\n            return g_cache[slot].fn;\n    }\n    return NULL;\n}\n\n/* L1 and PC-L0 are non-owning accelerators. They must lose every alias to an\n * owned generated body before that body is freed, otherwise cache pressure\n * would turn a later warm hit into a use-after-free jump. */\nstatic void ee_jit_cache_drop_fn_refs(ppc_block_fn victim)\n{\n    if (!victim) return;\n    for (uint32_t i = 0; i < EE_JIT_L1_SLOTS; i++) {\n        if (g_l1[i].fn == victim) memset(&g_l1[i], 0, sizeof(g_l1[i]));\n    }\n    for (uint32_t i = 0; i < EE_JIT_PC_L0_SLOTS; i++) {\n        if (g_pc_l0[i].fn == victim) memset(&g_pc_l0[i], 0, sizeof(g_pc_l0[i]));\n    }\n}\n\nstatic int ee_jit_cache_store(uint32_t instr, ppc_block_fn fn)\n{\n    if (!fn) return 0;\n    if (g_cache_count < EE_JIT_CACHE_SLOTS) {\n        uint32_t h = (instr * 2654435761u) & (EE_JIT_CACHE_SLOTS - 1u);\n        for (uint32_t probe = 0; probe < EE_JIT_CACHE_SLOTS; probe++) {\n            uint32_t slot = (h + probe) & (EE_JIT_CACHE_SLOTS - 1u);\n            if (g_cache[slot].fn == NULL) {\n                g_cache[slot].instr = instr;\n                g_cache[slot].fn = fn;\n                g_cache_count++;\n                return 1;\n            }\n        }\n        return 0;\n    }\n\n    /* A helper reached from generated code may re-enter the frontend. Never\n     * release any generated body while a single-op native body is on-stack. */\n    if (g_single_active) return 0;\n    uint32_t slot = g_cache_evict_cursor++ & (EE_JIT_CACHE_SLOTS - 1u);\n    ppc_block_fn victim = g_cache[slot].fn;\n    if (!victim) return 0; /* count==capacity makes this an invariant guard. */\n    ee_jit_cache_drop_fn_refs(victim);\n    g_cache[slot].instr = instr;\n    g_cache[slot].fn = fn;\n    g_cache_evictions++;\n    free((void *)victim);\n    return 1;\n}\n'''
once('source/core/recompiler/ee_jit.c', old_cache, new_cache)

once(
    'source/core/recompiler/ee_jit.c',
    '#endif /* GEKKO */',
    '#endif /* GEKKO || EE_JIT_CACHE_TEST */',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''        /* R1267: cached functions keep running when full; new encodings\n         * interpret instead of allocating unowned executable buffers. */\n        if (g_cache_count >= EE_JIT_CACHE_SLOTS) {\n            if (out_rejected) *out_rejected = 1;\n            return 0;\n        }\n        g_compile_attempts++;''',
    '''        /* R1318: full-cache replacement is safe only when no generated\n         * single-op body is currently on-stack. Treat re-entrant pressure as\n         * transient fallback, never as deterministic opcode rejection. */\n        if (g_cache_count >= EE_JIT_CACHE_SLOTS && g_single_active) return 0;\n        g_compile_attempts++;''',
)

once(
    'source/core/recompiler/ee_jit.c',
    'if (!ee_jit_cache_insert(instr, fn)) {',
    'if (!ee_jit_cache_store(instr, fn)) {',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''    if (out_fn) *out_fn = fn;\n    fn((ppc_dynarec_gpr128_t *)&st->gpr[0]);\n    g_jit_executed++;''',
    '''    if (out_fn) *out_fn = fn;\n    g_single_active++;\n    fn((ppc_dynarec_gpr128_t *)&st->gpr[0]);\n    g_single_active--;\n    g_jit_executed++;''',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''        if (e->fn) {\n            e->fn((ppc_dynarec_gpr128_t *)&st->gpr[0]);\n            g_pc_l0_hits++;''',
    '''        if (e->fn) {\n            g_single_active++;\n            e->fn((ppc_dynarec_gpr128_t *)&st->gpr[0]);\n            g_single_active--;\n            g_pc_l0_hits++;''',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''uint64_t ee_jit_get_executed_count(void) { return g_jit_executed; }\nuint32_t ee_jit_get_cache_size(void) { return g_cache_count; }\nuint64_t ee_jit_get_pc_l0_hit_count(void)''',
    '''uint64_t ee_jit_get_executed_count(void) { return g_jit_executed; }\nuint32_t ee_jit_get_cache_size(void) { return g_cache_count; }\nuint64_t ee_jit_get_eviction_count(void) { return g_cache_evictions; }\nuint64_t ee_jit_get_pc_l0_hit_count(void)''',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''void ee_jit_reset_stats_for_test(void)\n{\n    ee_precise_reset_cache();''',
    '''void ee_jit_reset_stats_for_test(void)\n{\n    if (g_single_active) return; /* Never free a generated body on-stack. */\n    ee_precise_reset_cache();''',
)

once(
    'source/core/recompiler/ee_jit.c',
    '''    g_cache_count = 0;\n    g_jit_executed = 0;''',
    '''    g_cache_count = 0;\n    g_cache_evict_cursor = 0;\n    g_cache_evictions = 0;\n    g_jit_executed = 0;''',
)

Path('tests/test_ee_jit_cache_eviction_r1318.c').write_text(r'''#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define EE_JIT_CACHE_TEST 1
#include "core/recompiler/ee_jit.c"

static int fail(const char *m) { fprintf(stderr, "R1318 cache eviction: %s\n", m); return 1; }

int main(void)
{
    ee_jit_reset_stats_for_test();
    for (uint32_t n = 0; n < EE_JIT_CACHE_SLOTS; n++) {
        ppc_block_fn fn = (ppc_block_fn)malloc(4u);
        if (!fn) return fail("allocation");
        if (!ee_jit_cache_store(0x10000000u + n, fn)) return fail("initial fill");
    }
    if (g_cache_count != EE_JIT_CACHE_SLOTS || g_cache_evictions != 0u)
        return fail("fill counters");

    uint32_t victim_slot = g_cache_evict_cursor & (EE_JIT_CACHE_SLOTS - 1u);
    uint32_t old_instr = g_cache[victim_slot].instr;
    ppc_block_fn victim = g_cache[victim_slot].fn;
    g_l1[7].instr = old_instr; g_l1[7].fn = victim;
    g_pc_l0[11].pc = 0x80001000u; g_pc_l0[11].instr = old_instr;
    g_pc_l0[11].fn = victim; g_pc_l0[11].rejected = 0;

    ppc_block_fn replacement = (ppc_block_fn)malloc(4u);
    if (!replacement) return fail("replacement allocation");
    const uint32_t new_instr = 0x7f123456u;
    if (!ee_jit_cache_store(new_instr, replacement)) return fail("replacement rejected");
    if (g_cache_evictions != 1u || g_cache_count != EE_JIT_CACHE_SLOTS)
        return fail("replacement counters");
    if (g_l1[7].fn || g_pc_l0[11].fn || g_pc_l0[11].rejected)
        return fail("non-owning alias survived eviction");
    if (ee_jit_cache_lookup(old_instr) != NULL)
        return fail("victim instruction still owned");
    if (ee_jit_cache_lookup(new_instr) != replacement)
        return fail("replacement lookup");

    ppc_block_fn blocked = (ppc_block_fn)malloc(4u);
    if (!blocked) return fail("blocked allocation");
    g_single_active = 1u;
    if (ee_jit_cache_store(0x7f654321u, blocked)) return fail("evicted while body active");
    g_single_active = 0u;
    free((void *)blocked); /* ownership was not transferred. */
    if (g_cache_evictions != 1u) return fail("active guard changed eviction count");

    ee_jit_reset_stats_for_test();
    if (g_cache_count || g_cache_evictions || g_cache_evict_cursor)
        return fail("reset bookkeeping");
    puts("R1318 cache eviction: PASS");
    return 0;
}
''')

notes = Path('docs/DYNAREC-WORKING-NOTES.md')
s = notes.read_text()
marker = '## R1318 — bounded single-op cache eviction'
if marker not in s:
    s += (
        '\n\n' + marker + '\n\n'
        'The 8192-entry instruction-keyed owning PPC cache now remains bounded under sustained opcode diversity '
        'without permanently forcing new encodings back to the interpreter. Full-cache replacement is round-robin; '
        'all non-owning L1 and PC-L0 aliases to a victim are cleared before its generated buffer is freed. A native '
        'single-op body is pinned while on-stack, so re-entrant helpers treat cache pressure as a transient scalar '
        'fallback instead of evicting executable memory. A host ownership test fills the entire cache, verifies alias '
        'invalidation/replacement lookup, exercises the active-body guard, and checks reset bookkeeping.\n'
    )
    notes.write_text(s)
