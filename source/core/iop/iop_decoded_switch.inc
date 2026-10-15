/* GPL-3.0-or-later. Shared scalar/AOT R3000A instruction bodies. */
    switch (op) {
    case 0x00: /* SPECIAL */
        switch (funct) {
        case 0x00: /* SLL */  if (rd) GPR(rd) = rt32 << sa; break;
        case 0x02: /* SRL */  if (rd) GPR(rd) = rt32 >> sa; break;
        case 0x03: /* SRA */  if (rd) GPR(rd) = (uint32_t)((int32_t)rt32 >> sa); break;
        case 0x04: /* SLLV */ if (rd) GPR(rd) = rt32 << (rs32 & 0x1F); break;
        case 0x06: /* SRLV */ if (rd) GPR(rd) = rt32 >> (rs32 & 0x1F); break;
        case 0x07: /* SRAV */ if (rd) GPR(rd) = (uint32_t)((int32_t)rt32 >> (rs32 & 0x1F)); break;
        case 0x08: /* JR */   BRANCH_TO(GPR(rs)); break;
        case 0x09: /* JALR */ {
            uint32_t tgt = GPR(rs);
            /* Round 77 (117th finding, task #221/#245): these two
             * exact addresses are the real ROM's own two device-table-
             * slot "call the real init function now" sites (see the
             * caller's disassembly in the 116th/117th findings -
             * 0xBFC4A39C for slot 1, 0xBFC4A44C for slot 2). $rs at
             * each of these sites holds a raw, NEVER-RELOCATED field
             * read straight out of the still-in-ROM ELF header (the
             * real ROM code assumes this device's image was already
             * ELF-loaded into RAM by an earlier step this project
             * doesn't model) - live tracing confirmed this resolves
             * to tiny, clearly-wrong addresses (0x890, 0x30) that
             * either read as zero-filled RAM (a stream of SLL $0,$0,0
             * NOPs) or immediately return, falling straight into the
             * "should never get here" panic trap this project's 92nd/
             * 116th findings already documented. Since we now know
             * the real raw entry (remembered from the matching JAL to
             * 0xBFC4A600 above) is a genuine IRX-format ELF image,
             * really ELF-load it here (reusing iop_elf.c's existing,
             * already-tested loader - the exact same machinery
             * iop_module_loader.c uses for every ROMDIR module) and
             * redirect the jump to its real, correctly-relocated
             * entry point instead. Fixed, generous load addresses
             * (0x001C0000/0x001C8000) were chosen well clear of both
             * the ROMDIR module loader's own bump-allocated region
             * (observed peak 0x00145CA0 in a full 29-module boot) and
             * the stack (0x001FFF00) - see the 117th finding for the
             * measurement. On any load failure, falls back to the
             * real ROM's own (buggy-today) value rather than risk a
             * worse outcome. Only active at PRId=0x1f - dead code at
             * the shipped default PRId=0. Verified via live host-
             * native tracing this DOES change behavior (IOP escapes
             * the exact literal panic-loop PC for the first time),
             * but also reveals a further, deeper blocker - see the
             * 117th finding's honest "not yet a working boot path"
             * section before treating this as more than a genuine
             * partial step. */
            if (st->cop0[15] == 0x1fu &&
                (this_pc == 0xBFC4A39Cu || this_pc == 0xBFC4A44Cu) &&
                st->devtable_pending_image != 0) {
                uint32_t raw = st->devtable_pending_image;
                st->devtable_pending_image = 0;
                if (raw >= 0xBFC00000u) {
                    uint32_t rom_off = raw - 0xBFC00000u;
                    if (st->bios && rom_off < st->bios->size) {
                        uint32_t image_size = st->bios->size - rom_off;
                        if (image_size > 0x8000u) image_size = 0x8000u;
                        uint32_t load_addr = (this_pc == 0xBFC4A39Cu)
                                             ? 0x001C0000u : 0x001C8000u;
                        iop_elf_load_result_t res;
                        const char *err = NULL;
                        int rc = iop_elf_load(st, st->bios->data + rom_off,
                                               image_size, load_addr, &res, &err);
                        if (rc == 0) {
                            tgt = res.entry;
                        }
                    }
                }
            }
            if (rd) LINK(rd); BRANCH_TO(tgt);
        } break;
        case 0x0C: /* SYSCALL - raises a real R3000A exception instead
             * of halting, ported from PCSX2's psxException()
             * (R3000A.cpp): Cause.ExcCode=8 (Syscall, pre-shifted
             * into bits 2-6 as 0x20), EPC=the SYSCALL instruction's
             * own address, PC vectors to 0xBFC00180 (bootstrap) or
             * 0x80000080 (normal) depending on Status.BEV (bit 22),
             * and the 3-level interrupt-enable/kernel-mode bit stack
             * (Status bits 0-5) shifts left by 2 (current->previous,
             * previous->old). NOTE: real hardware/PCSX2 also handles
             * the case where SYSCALL itself executes in a branch
             * delay slot (EPC=pc-4, Cause.BD=1 set) - not modeled
             * here, since this interpreter doesn't track per-step
             * delay-slot state; EPC is always set to the SYSCALL's
             * own address. A real SYSCALL landing in a delay slot is
             * rare in practice, but this is a known, documented
             * simplification, not an oversight. Unlike BREAK (below),
             * this does NOT halt the core - it's a real, successful
             * step, matching real hardware's actual behavior for this
             * instruction (BREAK is kept as this project's own
             * clean-halt-for-testing convention, not changed here). */
        {
            /* Round 29 continued (task #164): real IOP kernel syscall
             * numbers 0x10 and 0x08 - see docs/STATUS.md's 43rd
             * finding for the full derivation. Live-traced (this
             * project's own emulator, cross-checked register state at
             * the exact fault point) for every one of the 13 modules
             * that previously hit is_unconditional_trap_stub()
             * (SSBUSC, DMACMAN, THREADMAN, VBLANK, IOMAN, MODLOAD,
             * SIFCMD, CDVDMAN, SIFINIT: v0=0x10, a0=pointer to the
             * calling module's own local struct, a1=a fixed address
             * 0x00100030 or 0xBF801528 - consistent with a real
             * RegisterLibraryEntries-style kernel call; REBOOT,
             * LOADFILE, CDVDFSV, FILEIO: v0=0x08, a0=3 always).
             *
             * This project's exception vector (0x80000080/0xBFC00180)
             * has no real installed dispatcher for these - it's just
             * a default fallback stub baked into an early module's
             * own ELF segment data (42nd finding), which ends in an
             * unconditional TGE trap. Previously, this project's
             * module loader recognized that trap pattern and
             * abandoned the CALLING module's remaining execution
             * entirely, jumping to the NEXT module in the boot list
             * (is_unconditional_trap_stub()/advance_to_next_module()).
             * That bypass is kept for any OTHER syscall number that
             * still falls through to this same dead end, but for
             * these two specific, now-understood numbers, this
             * project instead applies the EXACT SAME precedent
             * already established for the A0/B0/C0 BIOS-table
             * convention (iop_hle_bios.c) and the BREAK-as-syscall-
             * fallback (tasks #149/#156): intercept BEFORE any real
             * exception is raised, return the same generic default
             * value (0) already used throughout this project for
             * unimplemented real kernel calls, and resume the CALLING
             * module's OWN code at the instruction right after the
             * syscall - so its real init can continue past this call
             * instead of being abandoned. No Cause/EPC/Status field is
             * touched for this path (matching the A0/B0/C0 HLE
             * convention: a pure software intercept, not a real
             * CPU-level exception at all - safe precisely because
             * these are scratch/kernel-internal fields the caller
             * never observes either way). Any OTHER syscall number is
             * completely unaffected and still raises the real
             * exception exactly as before. */
            uint32_t syscall_num = st->gpr[2]; /* $v0 - real IOP kernel syscall-number convention */
            /* Round 29 continued (task #164 continued): after adding
             * 0x10/0x08 handling above, live-traced (this project's
             * own emulator) every one of the remaining 12
             * trap-stub-bypassed modules and found ALL of them now
             * advance past their first syscall to a SECOND real
             * syscall, number 0x14 (a0=0 always; a1 varies per module
             * - a small index/priority-like value for most, one
             * larger address-like value for IOMAN - real semantics
             * not yet identified, e.g. could be a real
             * RegisterIntrHandler/CpuEnableIntr-style kernel call).
             * Same precedent, same generic default-return convention. */
            /* Task #217 (88th finding): real ps2sdk source
             * (iop/system/intrman/src/intrman.c, fetched and cited
             * this round) conclusively identifies syscall 0x08 as
             * `intrman_syscall_08_CpuEnableIntr` - the exact real
             * mechanism backing CpuEnableIntr(). Its real assembly
             * body (`syscall_handler_08_CpuEnableIntr` in the same
             * file's inline exception_system_handler_code) is:
             *   lw   $t0, 0x408($zero)   ; pre-exception Status,
             *                            ; saved by the generic ROM
             *                            ; exception-entry stub
             *   ori  $t0, $t0, 0x404     ; set bit2 (IEp) + bit10
             *                            ; (IM2, this project's
             *                            ; IOP_STATUS_IM2 - see
             *                            ; iop_check_hw_interrupt())
             *   mtc0 $t0, $12
             * ...followed by a real RFE at return (same file's
             * `syscall_handler_00_return_from_exception`, ends in
             * `.word 0x42000010` - COP0 RFE, exactly this project's
             * own already-implemented IOP RFE, task #113), which
             * shifts IEp into IEc. Net, real, documented effect once
             * CpuEnableIntr() returns: Status.IEc=1 and Status.IM2=1.
             *
             * This project's own diagnostic (STATUS.md's 87th
             * finding) proved Status stays 0x00000000 through the
             * entire real BIOS boot specifically because this exact
             * syscall was, until now, treated as a pure no-op by the
             * bypass below (inherited from task #164, which correctly
             * identified the CALLING convention - v0=8, module-
             * agnostic - but not yet this real target semantic).
             * Live-traced (this project's own module-completion
             * tracing, this round): THREADMAN's real _start() runs to
             * completion via a natural return and its own real last
             * action before returning is exactly this call
             * (ps2sdk's iop/system/threadman/src/thcommon.c _start(),
             * last line before `return MODULE_RESIDENT_END;`) -
             * confirming this is reachable, real, exercised code in
             * this project's own boot, not a hypothetical.
             *
             * 0x10 (CpuSuspendIntr) and 0x14 (CpuResumeIntr) are also
             * now known precisely (same file: 0x10 ANDs the saved
             * status with 0x414 as its return value then clears bits
             * 2/4/10; 0x14 clears those same bits then ORs in the
             * caller-supplied prior state) but are deliberately left
             * as before (pure no-ops, no Status effect) - real code
             * always pairs them 1:1 around a critical section, so
             * leaving both inert cannot itself cause a net IEc/IM2
             * change, and implementing only one half in an unproven
             * way risked *introducing* a spurious re-disable after
             * this fix's real CpuEnableIntr() runs. Not applying an
             * unverified fix without a proven failure is this
             * project's own established discipline (see docs/
             * STATUS.md's repeated "not fabricated without citation"
             * notes). */
            if (syscall_num == 0x08u) {
                st->pipe_write_reg=2;
                st->cop0[12] |= (0x1u | 0x400u); /* real net effect of CpuEnableIntr(): IEc (bit0) + IM2 (bit10) */
                st->gpr[2] = 0;
                st->pc = this_pc + 4u;
                st->next_pc = this_pc + 8u;
                break;
            }
            if (syscall_num == 0x10u || syscall_num == 0x14u) {
                st->pipe_write_reg=2;
                st->gpr[2] = 0; /* same generic default-return convention as iop_hle_bios.c / task #149/#156 */
                st->pc = this_pc + 4u;
                st->next_pc = this_pc + 8u;
                break;
            }
            iop_raise_sync(st,8u,0,0);
        }
        break;
        case 0x0D: /* BREAK */
            /* Round 29 continued (29th change): if this exact BREAK
             * is reached because a genuine R3000A hardware `syscall`
             * instruction (Cause.ExcCode==8) vectored to the general
             * exception handler and fell through its still-unclaimed
             * default chain (see docs/STATUS.md's 29th finding - this
             * is the SAME underlying architectural gap as task
             * #124/#132/#148: a later module hasn't yet installed a
             * real handler for this kernel-level syscall number,
             * because this project's loader runs one module's ELF
             * and entry point at a time), this project applies the
             * EXACT SAME precedent it already established throughout
             * iop_hle_bios.c for unimplemented A0/B0/C0 BIOS-table
             * calls: return a generic default value (0) to the
             * caller instead of halting. This is NOT a new pattern -
             * it's that same "unimplemented call returns 0" default,
             * extended from the BIOS-table mechanism to the real
             * hardware-level syscall/exception mechanism. Unlike
             * task #148's module-jump bypass, resuming after a
             * syscall is well-defined, ordinary MIPS exception-return
             * semantics (EPC+4, matching a real RFE-terminated
             * handler) - not a jump to a different, unrelated module,
             * so this carries none of that mechanism's own scoping
             * caveats.
             *
             * Any OTHER BREAK (Cause.ExcCode != 8 - e.g. this
             * project's own test suite's long-established direct
             * clean-halt-for-testing convention, which never first
             * executes a real syscall) is completely unaffected and
             * still halts exactly as before.
             *
             * Task #156 fix: this check now ALSO requires
             * exception_pending (see iop_core.h's field comment) -
             * i.e. that this Cause.ExcCode==8 reflects a syscall
             * exception that hasn't been handled (RFE'd) yet, not a
             * stale leftover value from an EARLIER syscall whose
             * handler already ran RFE and returned. Without this,
             * a genuinely-real, distinct BREAK reached later (e.g.
             * this project's own test suite's clean-halt convention,
             * or any real BREAK downstream of an unrelated, already-
             * completed syscall) was wrongly treated as "still
             * unhandled" and resumed at the OLD, already-stale EPC+4
             * instead of halting - a real, reproducible infinite loop
             * (see tests/test_iop_rfe.c: SYSCALL, then RFE at the
             * bootstrap vector, then BREAK - Cause still read 8 since
             * RFE never touches Cause, so this fired incorrectly and
             * resumed execution back near the reset vector's own
             * zeroed/NOP-equivalent memory, which never halts). */
            if (st->exception_pending && (st->cop0[13] & 0x7Cu) == 0x20u) { /* Cause.ExcCode == 8 (Syscall), not yet RFE'd */
                uint32_t epc = st->cop0[14];
                st->gpr[2] = 0; /* $v0 = 0 - same default-return convention as iop_hle_bios.c's unimplemented A0/B0/C0 calls */
                /* RFE-equivalent Status stack pop, identical formula
                 * to this file's own real RFE (COP0 CO-format
                 * funct=0x10) implementation above. */
                st->cop0[12] = (st->cop0[12] & ~0x0Fu) | ((st->cop0[12] & 0x3Cu) >> 2);
                st->exception_pending = 0; /* task #156 - this exception is now considered handled */
                st->pc = (st->cop0[13] & IOP_CAUSE_BD) ? st->cop0[6] : epc + 4u;
                st->next_pc = st->pc + 4u;
                break;
            }
            /* Round 752 (task #735): real R3000A/MIPS I hardware never
             * stops executing just because it decoded a BREAK - it
             * raises a genuine Breakpoint exception (ExcCode 9) and
             * vectors through the normal exception path, exactly like
             * every other trap-like class already implemented in this
             * file (SYSCALL above, TGE/Trap and Reserved Instruction
             * below). This project's own EE core already made this
             * exact fix (see ee_core.c's SPECIAL funct 0x0D case,
             * task #178) after finding a real, intentional BREAK
             * physically present in the BIOS image - the unconditional
             * halt() here was the same kind of pragmatic placeholder,
             * predating real IOP exception delivery, that task #178
             * already identified and fixed on the EE side.
             *
             * Round 751's major finding motivates fixing this now: a
             * real, ordinary divide-by-zero guard idiom in genuine IOP
             * kernel/module code (0x00119650-0x0011977C, reached deep
             * in the GT3 checkpoint chain past 6B instructions) hits
             * this BREAK when the divisor is zero. Real hardware would
             * vector to the kernel's installed Breakpoint handler
             * (which may resume past it, matching common real debug-
             * trap semantics) rather than permanently killing the IOP
             * core - which is exactly what was silently happening
             * here, deadlocking every EE-side SBUS mailbox-flag poll
             * loop that depends on the IOP staying alive.
             *
             * Mirrors this file's own established general-exception-
             * vector mechanism exactly (EPC=this instruction's own
             * address, PC vectors to 0xBFC00180/0x80000080 depending
             * on Status.BEV, same Status KU/IE stack left-shift-by-2
             * push) - identical in form to the SYSCALL/TGE/Reserved-
             * Instruction cases above/below, just ExcCode=9 instead. */
            iop_raise_sync(st,9u,0,0);
            break;
        case 0x10: /* MFHI */ if (rd) GPR(rd) = st->hi; break;
        case 0x11: /* MTHI */ st->hi = GPR(rs); break;
        case 0x12: /* MFLO */ if (rd) GPR(rd) = st->lo; break;
        case 0x13: /* MTLO */ st->lo = GPR(rs); break;
        case 0x18: /* MULT */ {
            int64_t res = (int64_t)(int32_t)rs32 * (int64_t)(int32_t)rt32;
            st->lo = (uint32_t)(res & 0xFFFFFFFFu);
            st->hi = (uint32_t)(res >> 32);
        } break;
        case 0x19: /* MULTU */ {
            uint64_t res = (uint64_t)rs32 * (uint64_t)rt32;
            st->lo = (uint32_t)(res & 0xFFFFFFFFu);
            st->hi = (uint32_t)(res >> 32);
        } break;
        case 0x1A: /* DIV: defined R3000A zero/overflow outcomes, primary
                     * R3000AOpcodeTables.cpp psxDIV, no host C overflow. */
            if (!rt32) { st->lo=(int32_t)rs32<0?1u:0xffffffffu;st->hi=rs32; }
            else if(rs32==0x80000000u&&rt32==0xffffffffu){st->lo=0x80000000u;st->hi=0;}
            else {st->lo=(uint32_t)((int32_t)rs32/(int32_t)rt32);st->hi=(uint32_t)((int32_t)rs32%(int32_t)rt32);}
            break;
        case 0x1B: /* DIVU */
            if(!rt32){st->lo=0xffffffffu;st->hi=rs32;}
            else {st->lo=rs32/rt32;st->hi=rs32%rt32;}
            break;
        case 0x20: /* ADD */
        case 0x21: /* ADDU */ if (rd) GPR(rd) = rs32 + rt32; break;
        case 0x22: /* SUB */
        case 0x23: /* SUBU */ if (rd) GPR(rd) = rs32 - rt32; break;
        case 0x24: /* AND */  if (rd) GPR(rd) = rs32 & rt32; break;
        case 0x25: /* OR */   if (rd) GPR(rd) = rs32 | rt32; break;
        case 0x26: /* XOR */  if (rd) GPR(rd) = rs32 ^ rt32; break;
        case 0x27: /* NOR */  if (rd) GPR(rd) = ~(rs32 | rt32); break;
        case 0x2A: /* SLT */  if (rd) GPR(rd) = ((int32_t)rs32 < (int32_t)rt32) ? 1 : 0; break;
        case 0x2B: /* SLTU */ if (rd) GPR(rd) = (rs32 < rt32) ? 1 : 0; break;
        case 0x30: /* TGE - Trap if Greater or Equal (signed). Task #150:
             * this opcode was first observed as an "unimplemented
             * SPECIAL funct 0x30" halt at pc=0x800000AC, reached after
             * task #149's syscall-return fix let a second real syscall
             * fall through the still-unclaimed general exception
             * vector and re-walk that low-memory region. Real MIPS
             * trap semantics (condition-taken raises a Trap exception,
             * ExcCode=13 pre-shifted to bits 2-6 as 0x34; condition-
             * not-taken is a pure no-op with no side effects at all -
             * not even implicitly falling through like a branch, since
             * there's no delay slot for trap instructions). Delivery
             * mirrors this file's own existing SYSCALL exception path
             * exactly (EPC=this instruction's own address, PC vectors
             * to 0xBFC00180/0x80000080 depending on Status.BEV, same
             * Status KU/IE stack left-shift-by-2 push) - the same
             * real R3000A exception-delivery mechanism, just a
             * different ExcCode and trigger condition. */
            if ((int32_t)rs32 >= (int32_t)rt32) {
                iop_raise_sync(st,13u,0,0);
            }
            break;
        default:
            /* Round 175 (task #340, 215th finding): real R3000A/MIPS
             * I hardware does not halt when it decodes an undefined
             * SPECIAL funct - it raises a genuine Reserved
             * Instruction exception (ExcCode 0x0A, a universal, publicly
             * documented base MIPS I architecture feature - System V
             * ABI MIPS Processor Supplement / any public R3000A
             * reference - not specific to any BIOS content). Delivery
             * mirrors this file's own existing SYSCALL/TRAP exception
             * paths exactly (same EPC/vector/Status-stack-push
             * mechanism, already implemented and tested above) - the
             * only difference is the ExcCode value and trigger
             * condition. Reached via real, genuinely-executing module
             * code after Round 174's interrupt-dispatch fix let
             * execution reach further than ever before (pc=0x00117CC4,
             * per docs/STATUS.md's 215th finding) - rather than assume
             * what SHOULD be at this address (which would require
             * transcribing/guessing at real BIOS content, against this
             * project's standing clean-room convention), this project
             * now does what real hardware actually does: delivers a
             * real exception and lets whatever real handler chain is
             * already installed (or this project's own already-
             * existing default-vector fallback, extended below to
             * treat ExcCode 0x0A as synchronous/non-restartable, same
             * as SYSCALL/BREAK/Trap) decide what happens next. This
             * replaces an immediate, uninformative halt with the
             * architecturally-correct real behavior, and may reveal
             * further genuine progress or a cleaner subsequent wall
             * instead of guessing opcode-by-opcode. */
            iop_raise_sync(st,10u,0,0);
            break;
        }
        break;

    case 0x01: /* REGIMM */
        switch (rt) {
        case 0x00: /* BLTZ */   if ((int32_t)GPR(rs) < 0)  BRANCH_TO(this_pc + 4 + (imm << 2)); break;
        case 0x01: /* BGEZ */   if ((int32_t)GPR(rs) >= 0) BRANCH_TO(this_pc + 4 + (imm << 2)); break;
        case 0x10: /* BLTZAL */ LINK(31); if ((int32_t)GPR(rs) < 0)  BRANCH_TO(this_pc + 4 + (imm << 2)); break;
        case 0x11: /* BGEZAL */ LINK(31); if ((int32_t)GPR(rs) >= 0) BRANCH_TO(this_pc + 4 + (imm << 2)); break;
        default:
            /* Round 175 (task #340, 215th finding) - same real
             * Reserved Instruction exception (ExcCode 0x0A) as the
             * SPECIAL default case above; see that case's own comment
             * for the full citation trail. */
            iop_raise_sync(st,10u,0,0);
            break;
        }
        break;

    case 0x02: /* J */   BRANCH_TO(((this_pc + 4u) & 0xF0000000u) | ((instr & 0x03FFFFFFu) << 2)); break;
    case 0x03: /* JAL */ {
        uint32_t jal_tgt = ((this_pc + 4u) & 0xF0000000u) | ((instr & 0x03FFFFFFu) << 2);
        /* Round 77 (117th finding, task #221/#245): 0xBFC4A600 is the
         * real ROM's device-table-entry classifier (see the 116th/
         * 117th findings) - it takes the raw device-table entry
         * pointer in $a0. That raw entry turned out to be a genuine
         * embedded ELF32/MIPS IOP module image (same "IRX" format
         * iop_elf.c already loads for ROMDIR-listed modules - real
         * e_machine=8, vendor e_type=0xFF80, PT_MIPS_IOPMOD segment).
         * Remember it here so the matching JALR call site below (the
         * real ROM code's own "call this device's real init function"
         * step) can ELF-load it for real instead of jumping to
         * whatever raw, unrelocated header field the ROM's un-loaded
         * pointer happens to contain. Only active when PRId is the
         * real retail value - completely inert (zero behavior change)
         * at the default PRId=0, since this ROM code path is never
         * even reached then (see g_iop.cop0[15]'s own header comment). */
        if (st->cop0[15] == 0x1fu && jal_tgt == 0xBFC4A600u) {
            st->devtable_pending_image = GPR(4);
        }
        LINK(31); BRANCH_TO(jal_tgt);
    } break;
    case 0x04: /* BEQ */  if (GPR(rs) == GPR(rt)) BRANCH_TO(this_pc + 4 + (imm << 2)); break;
    case 0x05: /* BNE */  if (GPR(rs) != GPR(rt)) BRANCH_TO(this_pc + 4 + (imm << 2)); break;
    case 0x06: /* BLEZ */ if ((int32_t)GPR(rs) <= 0) BRANCH_TO(this_pc + 4 + (imm << 2)); break;
    case 0x07: /* BGTZ */ if ((int32_t)GPR(rs) > 0)  BRANCH_TO(this_pc + 4 + (imm << 2)); break;

    case 0x08: /* ADDI */
    case 0x09: /* ADDIU */ if (rt) GPR(rt) = rs32 + (uint32_t)imm; break;
    case 0x0A: /* SLTI */  if (rt) GPR(rt) = ((int32_t)GPR(rs) < imm) ? 1 : 0; break;
    case 0x0B: /* SLTIU */ if (rt) GPR(rt) = (GPR(rs) < (uint32_t)imm) ? 1 : 0; break;
    case 0x0C: /* ANDI */  if (rt) GPR(rt) = GPR(rs) & uimm; break;
    case 0x0D: /* ORI */   if (rt) GPR(rt) = GPR(rs) | uimm; break;
    case 0x0E: /* XORI */  if (rt) GPR(rt) = GPR(rs) ^ uimm; break;
    case 0x0F: /* LUI */   if (rt) GPR(rt) = uimm << 16; break;

    case 0x10: /* COP0 */
        switch (rs) {
        case 0x00: /* MFC0 */ if (rt) GPR(rt) = st->cop0[rd]; break;
        case 0x04: /* MTC0 */ st->cop0[rd] = rt32; break;
        case 0x10: /* CO-format (bit 25 set, rs=0x10, rest of rs
             * field zero in the real encoding) - real R3000A/R5900
             * "cofun" ops selected by the low 6 bits (funct). Only
             * RFE (funct=0x10) is real R3000A architecture (no TLB
             * on the IOP, unlike the EE, so TLBR/TLBWI/TLBWR/TLBP
             * are not applicable here and are correctly left
             * unimplemented). This was a genuine, confirmed gap
             * found while investigating why Status.IEc never
             * becomes 1 (see docs/STATUS.md "Round 22"): every real
             * MIPS I exception handler ends in RFE to restore the
             * pre-exception KU/IE mode stack (and re-enable
             * interrupts if they were enabled beforehand) before
             * returning via JR - without this, any real handler
             * that actually completes and returns would have hit
             * this same "unimplemented COP0 sub-opcode" halt below,
             * silently masking whatever the real RAM[0x100] handler
             * chain does after being fixed. Ported from PCSX2's
             * R3000A.cpp psxException()'s RFE case:
             * `Status = (Status & ~0xF) | ((Status & 0x3C) >> 2)`
             * - shifts the 2-bit-pair KU/IE "previous" and "old"
             * fields down into "current"/"previous", leaving the
             * top "old" pair (bits 4-5) untouched, mirroring the
             * exception-entry push this project already implements
             * (`(cop0[12] & 0x0F) << 2` into bits 2-5, clearing 0-1). */
            switch (funct) {
            case 0x10: /* RFE */
                st->cop0[12] = (st->cop0[12] & ~0x0Fu) | ((st->cop0[12] & 0x3Cu) >> 2);
                /* Task #156: this exception is now considered handled
                 * - see iop_core.h's exception_pending field comment
                 * and BREAK's own updated check below. */
                st->exception_pending = 0;
                break;
            default:
                /* Round 175 (task #340, 215th finding) - same real
                 * Reserved Instruction exception (ExcCode 0x0A) as
                 * the SPECIAL/REGIMM/primary-opcode default cases
                 * elsewhere in this function; see the SPECIAL default
                 * case's own comment for the full citation trail.
                 * Only RFE (funct=0x10) is real R3000A CO-format
                 * architecture (no TLB on the IOP, unlike the EE) -
                 * any other funct value here is genuinely undefined
                 * encoding space, the same class of gap as an
                 * unimplemented SPECIAL/REGIMM funct. */
                iop_raise_sync(st,10u,0,0);
                break;
            }
            break;
        default:
            /* Round 175 (task #340, 215th finding) - same real
             * Reserved Instruction exception; see the SPECIAL default
             * case's own comment for the full citation trail. Real
             * COP0 only defines rs=0x00 (MFC0), 0x04 (MTC0), and
             * 0x10 (CO-format) on the R3000A - any other rs value is
             * genuinely undefined encoding space. */
            iop_raise_sync(st,10u,0,0);
            break;
        }
        break;

    case 0x20: /* LB */  if (rt) GPR(rt) = (uint32_t)(int32_t)(int8_t)iop_mem_read8(st, rs32 + imm); else iop_mem_read8(st, rs32 + imm); break;
    case 0x21: /* LH */  if (rt) GPR(rt) = (uint32_t)(int32_t)(int16_t)iop_mem_read16(st, rs32 + imm); else iop_mem_read16(st, rs32 + imm); break;
    case 0x23: /* LW */  if (rt) GPR(rt) = iop_mem_read32(st, rs32 + imm); else iop_mem_read32(st, rs32 + imm); break;
    case 0x24: /* LBU */ if (rt) GPR(rt) = iop_mem_read8(st, rs32 + imm); else iop_mem_read8(st, rs32 + imm); break;
    case 0x25: /* LHU */ if (rt) GPR(rt) = iop_mem_read16(st, rs32 + imm); else iop_mem_read16(st, rs32 + imm); break;

    case 0x22: /* LWL */ {
        uint32_t addr = rs32 + imm;
        uint32_t shift = (addr & 3) << 3;
        uint32_t mem = iop_mem_read32(st, addr & ~3u);
        if (rt) GPR(rt) = (rt32 & (0x00FFFFFFu >> shift)) | (mem << (24 - shift));
    } break;
    case 0x26: /* LWR */ {
        uint32_t addr = rs32 + imm;
        uint32_t shift = (addr & 3) << 3;
        uint32_t mem = iop_mem_read32(st, addr & ~3u);
        if (rt) GPR(rt) = (rt32 & (0xFFFFFF00u << (24 - shift))) | (mem >> shift);
    } break;

    case 0x28: /* SB */ iop_mem_write8(st, rs32 + imm, (uint8_t)GPR(rt)); break;
    case 0x29: /* SH */ iop_mem_write16(st, rs32 + imm, (uint16_t)GPR(rt)); break;
    case 0x2B: /* SW */ iop_mem_write32(st, rs32 + imm, GPR(rt)); break;

    case 0x2A: /* SWL */ {
        uint32_t addr = rs32 + imm;
        uint32_t shift = (addr & 3) << 3;
        uint32_t mem = iop_mem_read32(st, addr & ~3u);
        iop_mem_write32(st, addr & ~3u, (rt32 >> (24 - shift)) | (mem & (0xFFFFFF00u << shift)));
    } break;
    case 0x2E: /* SWR */ {
        uint32_t addr = rs32 + imm;
        uint32_t shift = (addr & 3) << 3;
        uint32_t mem = iop_mem_read32(st, addr & ~3u);
        iop_mem_write32(st, addr & ~3u, (rt32 << shift) | (mem & (0x00FFFFFFu >> (24 - shift))));
    } break;

    /* Round 128 (task #172/#196, 168th finding): CACHE (primary
     * opcode 0x2F) - standard MIPS I/II instruction (public ISA,
     * independent of any BIOS-specific data), real IOP kernel code
     * issues it routinely (e.g. around DMA buffer boundaries) to
     * invalidate/writeback cache lines. This project models no real
     * instruction/data cache at all for either core (already-
     * established, honestly-scoped simplification used throughout -
     * same rationale as the "no cycle-accurate timing" scope), so
     * CACHE is architecturally correct as a pure no-op here: with no
     * cache to act on, there is nothing for it to do. Found via
     * host-native instrumentation showing the IOP halting on this
     * exact opcode immediately after Round 127's timing fix unblocked
     * further boot progress. */
    case 0x2F: /* CACHE */ break;

    default:
        /* Round 175 (task #340, 215th finding) - same real Reserved
         * Instruction exception (ExcCode 0x0A) as the SPECIAL/REGIMM
         * default cases above; see the SPECIAL default case's own
         * comment for the full citation trail. */
        iop_raise_sync(st,10u,0,0);
        break;
    }
