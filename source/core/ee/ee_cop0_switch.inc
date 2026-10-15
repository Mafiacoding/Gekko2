/* GPL-3.0-or-later. Shared scalar/AOT semantics from ee_core.c. */
    case 0x10: /* COP0 */
        switch (rs) {
        case 0x00: /* MFC0 */
            if (rt) GPR(rt) = sext32(st->cop0[rd]);
            break;
        case 0x04: /* MTC0 */
            if (rd == 16) { /* Config: preserve read-only IC/DC bits like PCSX2's WriteCP0Config */
                st->cop0[16] = (rt32 & ~0xFC0u) | 0x440u;
            } else if (rd == 11) { /* Compare: writing it clears the latched
                 * timer-interrupt pending bit (Cause.IP7) - see
                 * ee_latch_timer_interrupt()'s comment above (round 9). */
                st->cop0[11] = rt32;
                st->cop0[13] &= ~EE_CAUSE_IP7;
            } else {
                st->cop0[rd] = rt32;
                if (rd == 10) ee_jit_notify_mapping_change(); /* EntryHi/ASID */
            }
            break;
        case 0x08: { /* BC0F/T/FL/TL: real DMA completion condition. */
            /* PCSX2 COP0.cpp::CPCOND0 and Dmac.h: CIS and CPC both
             * occupy bits 0..9. Every selected completion must be set. */
            dma_state_t *d = dma_get_state();
            int condition = ((d->d_stat | ~d->d_pcr) & 0x3ffu) == 0x3ffu;
            unsigned kind = rt & 3u;
            int taken = condition == (int)(kind & 1u);
            if (taken) BRANCH_TO(this_pc + 4u + (uint32_t)(imm * 4));
            else if (kind & 2u) {
                st->pc = fallthrough_pc + 4u;
                st->next_pc = fallthrough_pc + 8u;
            } else st->branch_pending = 1u;
            break;
        }
        default:
            if (rs & 0x10) {
                /* "CO" format: rs's top bit set means the real
                 * operation is selected by the 6-bit funct field
                 * (bits 0-5), not by rs itself - matches PCSX2's own
                 * tbl_COP0_C0[64] dispatch table in
                 * R5900OpcodeTables.cpp. */
                uint32_t co_funct = instr & 0x3F;
                switch (co_funct) {
                case 0x10: /* RFE - "Return From Exception".
                     * NOTE: real PCSX2's EE (R5900) opcode table does
                     * NOT implement this (tbl_COP0_C0[0x10] is
                     * COP0_Unknown there) - RFE is the ORIGINAL MIPS I
                     * exception-return instruction, superseded by
                     * ERET (funct 0x18, below) on MIPS III+. It shows
                     * up here because this real BIOS dump's early
                     * boot code includes PS1-backward-compatibility-
                     * mode boot code (this file's own header bytes
                     * contain the ASCII string "PS compatible mode"),
                     * which runs in a MIPS-I-compatible execution
                     * context where RFE is meaningful. Implemented
                     * with RFE's standard, unambiguous MIPS I
                     * semantics (restore the 3-level interrupt-
                     * enable/kernel-mode bit stack by shifting it
                     * right by 2) rather than skipped/faked, since
                     * that's well-defined regardless of the exact
                     * execution mode question above. RFE does NOT
                     * itself change PC (unlike ERET) - the caller
                     * separately jumps to EPC, typically via JR. */
                    st->cop0[12] = (st->cop0[12] & ~0x0Fu) | ((st->cop0[12] >> 2) & 0x0Fu);
                    break;
                case 0x18: /* ERET - ported from PCSX2's COP0::ERET().
                     * No branch delay slot (unlike ordinary jumps/
                     * branches) - takes effect immediately, so this
                     * sets st->pc/next_pc directly rather than going
                     * through the BRANCH_TO() delay-slot convention. */
                {
                    uint32_t target;
                    r1189_eret_count++;
                    uint32_t r1126_epc = st->cop0[14];
                    uint32_t r1126_status_before = st->cop0[12];
                    if (st->cop0[12] & 0x4u) { /* Status.ERL */
                        target = st->cop0[30]; /* ErrorEPC */
                        st->cop0[12] &= ~0x4u;
                    } else {
                        target = st->cop0[14]; /* EPC */
                        st->cop0[12] &= ~0x2u; /* Status.EXL */
                    }
                    st->pc = target;
                    st->next_pc = target + 4;
                    /* Round 598 (task #447/#536): forced preemption now
                     * fires here, right after a real interrupt/exception
                     * return, instead of on every instruction (see the
                     * long comment at this function's other former call
                     * site in ee_step()'s epilogue for the full Round 598
                     * regression writeup). This is the one point where
                     * real EE hardware's own kernel-level forced
                     * preemption genuinely happens - the interrupt-return
                     * path re-checking the ready queue before restoring
                     * a thread's context - so this is a strictly more
                     * hardware-faithful integration point, not just a
                     * regression workaround. */
#ifdef R1126_ERET_TRACE
                    {
                        int r1126_old_tid = ee_hle_thread_get_current_thread_id();
                        ee_hle_thread_check_preempt(st);
                        int r1126_new_tid = ee_hle_thread_get_current_thread_id();
                        uint32_t r1126_new_status = ee_hle_thread_get_status(r1126_new_tid);
                        fprintf(stderr, "[R1126ERET] EPC=0x%08x target_pc=0x%08x old_tid=%d next_tid=%d next_pc=0x%08x status_before=0x%08x status_after=0x%08x next_status=0x%08x\n",
                                r1126_epc, target, r1126_old_tid, r1126_new_tid, st->pc,
                                r1126_status_before, st->cop0[12], r1126_new_status);
                    }
#else
                    ee_hle_thread_check_preempt(st);
#endif
                    break;
                }
                case 0x38: /* EI - ported from PCSX2's COP0::EI(). Gated
                     * the same way real hardware/PCSX2 gates it:
                     * only takes effect if _EDI, EXL, ERL are set, or
                     * KSU==0 (kernel mode) - i.e. code running in
                     * user mode without the right permission bit
                     * can't enable interrupts via this instruction. */
                    if ((st->cop0[12] & 0x20000u) ||      /* _EDI  (bit 17) */
                        (st->cop0[12] & 0x2u) ||           /* EXL   (bit 1)  */
                        (st->cop0[12] & 0x4u) ||           /* ERL   (bit 2)  */
                        ((st->cop0[12] & 0x18u) == 0))      /* KSU==0 (bits 3-4) */
                    {
                        st->cop0[12] |= 0x10000u; /* Status.EIE = 1 (bit 16) */
                    }
                    break;
                case 0x39: /* DI - ported from PCSX2's COP0::DI(), same gating as EI. */
                    if ((st->cop0[12] & 0x20000u) ||
                        (st->cop0[12] & 0x2u) ||
                        (st->cop0[12] & 0x4u) ||
                        ((st->cop0[12] & 0x18u) == 0))
                    {
                        st->cop0[12] &= ~0x10000u; /* Status.EIE = 0 */
                    }
                    break;
                case 0x01: /* TLBR - Read Indexed TLB Entry. Ported from
                     * PCSX2's COP0::TLBR(). Loads the TLB entry at
                     * Index (cop0[0], masked to 6 bits/48 entries)
                     * back into PageMask/EntryHi/EntryLo0/EntryLo1
                     * (cop0[5]/[10]/[2]/[3]), applying the same
                     * read-back masking real hardware does (G is only
                     * set in the read-back copies if BOTH EntryLo0 and
                     * EntryLo1 have G set in the stored entry - see
                     * PCSX2's comment on this exact quirk). */
                {
                    uint32_t i = st->cop0[0] & 0x3Fu;
                    if (i > 47) break; /* real hardware/PCSX2 both just warn and no-op past entry 47 */
                    uint32_t lo0 = st->tlb[i].entry_lo0, lo1 = st->tlb[i].entry_lo1;
                    uint32_t g = (lo0 & 1u) & (lo1 & 1u);
                    st->cop0[5]  = st->tlb[i].page_mask;
                    st->cop0[10] = st->tlb[i].entry_hi & ~((st->tlb[i].page_mask) | 0x1F00u);
                    st->cop0[2]  = (lo0 & ~0xFC000000u & ~1u) | g;
                    st->cop0[3]  = (lo1 & ~0x7C000000u & ~1u) | g;
                    ee_jit_notify_mapping_change(); /* TLBR changes current EntryHi/ASID. */
                    break;
                }
                case 0x02: /* TLBWI - Write Indexed TLB Entry. Ported
                     * from PCSX2's COP0::TLBWI()/WriteTLB(). Stores
                     * the current PageMask/EntryHi/EntryLo0/EntryLo1
                     * into the TLB at Index. NOTE: this project does
                     * not wire TLB lookups into actual address
                     * translation (ee_mem_ptr() still treats kuseg/
                     * kseg0/kseg1 as flat physical-masked mappings) -
                     * this stores the entry faithfully so MFC0/TLBR/
                     * TLBP round-trip correctly, matching real
                     * hardware's register-level behavior, without
                     * claiming to model the MMU's actual page-walk. */
                {
                    uint32_t j = st->cop0[0] & 0x3Fu;
                    if (j > 47) break;
                    st->tlb[j].page_mask = st->cop0[5];
                    st->tlb[j].entry_hi  = st->cop0[10];
                    st->tlb[j].entry_lo0 = st->cop0[2];
                    st->tlb[j].entry_lo1 = st->cop0[3];
                    ee_jit_notify_mapping_change();
                    break;
                }
                case 0x06: /* TLBWR - Write Random TLB Entry. Same as
                     * TLBWI but indexed by Random (cop0[1]) instead of
                     * Index. Real hardware/PCSX2 decrement Random
                     * every cycle between Wired and 47; this project
                     * does not model that decay (Random is just
                     * whatever value software last wrote via MTC0, or
                     * 0) - a documented simplification, not a
                     * fabricated behavior, since we don't claim
                     * cycle-accurate Random decay. */
                {
                    uint32_t j = st->cop0[1] & 0x3Fu;
                    if (j > 47) break;
                    st->tlb[j].page_mask = st->cop0[5];
                    st->tlb[j].entry_hi  = st->cop0[10];
                    st->tlb[j].entry_lo0 = st->cop0[2];
                    st->tlb[j].entry_lo1 = st->cop0[3];
                    ee_jit_notify_mapping_change();
                    r1191_tlbwr_count++;
                    break;
                }
                case 0x08: /* TLBP - Probe TLB for Matching Entry.
                     * Ported from PCSX2's COP0::TLBP(). Searches all
                     * 48 entries for one whose (masked) VPN2 and
                     * ASID/Global match the current EntryHi, sets
                     * Index to the match (or 0x80000000, i.e. sign bit
                     * set, if none found - the real "not found"
                     * convention, checked via MFC0 $rt,$0 returning a
                     * negative value). */
                {
                    uint32_t eh = st->cop0[10];
                    uint32_t want_vpn2 = (eh >> 13) & 0x7FFFFu;
                    uint32_t want_asid = eh & 0xFFu;
                    uint32_t found = 0xFFFFFFFFu;
                    for (uint32_t i = 0; i < 48; i++) {
                        uint32_t mask = (st->tlb[i].page_mask >> 13) & 0xFFFu;
                        uint32_t entry_vpn2 = (st->tlb[i].entry_hi >> 13) & 0x7FFFFu;
                        int is_global = (st->tlb[i].entry_lo0 & 1u) && (st->tlb[i].entry_lo1 & 1u);
                        uint32_t entry_asid = st->tlb[i].entry_hi & 0xFFu;
                        if ((entry_vpn2 & ~mask) == (want_vpn2 & ~mask) &&
                            (is_global || entry_asid == want_asid)) {
                            found = i;
                            break;
                        }
                    }
                    st->cop0[0] = (found == 0xFFFFFFFFu) ? 0x80000000u : found;
                    break;
                }
                default:
                {
                    char buf[96];
                    snprintf(buf, sizeof(buf),
                             "unimplemented COP0 CO-format funct 0x%02X (pc=0x%08X)",
                             (unsigned int)co_funct, (unsigned int)this_pc);
                    halt(buf);
                    return 1;
                }
                }
            } else {
                char buf[96];
                snprintf(buf, sizeof(buf),
                         "unimplemented COP0 sub-opcode (rs=0x%02X, pc=0x%08X)",
                         (unsigned int)rs, (unsigned int)this_pc);
                halt(buf);
                return 1;
            }
            break;
        }
        break;
