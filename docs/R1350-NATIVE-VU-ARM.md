# R1350 Beta

Native execution: CVT.S.W uses biased IEEE double construction, direct PPC fsubs and stfs. No C conversion call remains in that generated body. This emitter also qualifies for guarded native EE event windows. Device MMIO, exceptions, runtime scheduling and remaining instruction helpers are not magically converted into native guest code.

VU correction: LQ/SQ/LQI/SQI/LQD/SQD honor all four component-mask bits. SQI/SQD address and increment/decrement It (bits16..20), read Fs (bits11..15), and retain unselected memory lanes. Shared pair conflict policy now tracks Fs for those stores. Both C and native emitters were corrected. Reference: local PCSX2 VUops.cpp _vuLQ/_vuSQ/_vuLQI/_vuSQI/_vuLQD/_vuSQD, and the independently specified mask/old-Fs test vectors. These errors can affect graphics packet production; elimination of the captured zero-vertex BIOS problem still needs Wii verification.

ARM reply: static aligned32 capability buffer; flush initialization, synchronous ioctl, invalidate before reading. ARM_WORKER_CAPS records every BE word and decline reason:0 accepted,1 length/result,2 magic,3 version,4 CSC flag,5 block limit. No malformed reply is accepted. The exact shipped93764-byte big-endian ARM ELF was executed in Unicorn ARM and returns32 with words474b4131,1,1,8,0,0,0,0. IOS cache operations are mocked there; physical Wii cache/IPC behavior remains to be measured.

## DSI fastmem audit

Audited nullDC4Wii commit26a623e3feca9927d8ffe43706b06d82d672dd64, wii/wii_fastmem.h and.cpp. It uses SR0/SR1 with a hashed page table and a checked512MB window; its exception glue delegates recognized generated sites to rec_fastmem_patch. MMIO accesses execute in normal-context trampolines after returning from the exception, never inside the DSI handler. Its current exception glue assumes the newer PPCExcptSetHandler contract. Our supplied libogc1.8.18 exposes __exception_sethandler and uses its older assembly/frame contract. Direct copying would be incorrect.

R1350 does not install a DSI handler or enable MMU fault-based memory access. Required next work: old-libogc saved-context adapter and precise exception chaining; isolated page window and host RAM mapping; per-site store/load descriptors and normal-context fallback trampolines; cache-coherent instruction patching; source/TLB/ASID/DMA invalidation and self-test; feature-gated Wii validation. Existing guarded RAM fastmem is retained. No guest underclock or arbitrary IOP idle skipping was added.

## Verification

Final linked Wii ELF:2092 signed CVT conversion cases;70 complete native/precise window state cases and seven declines;192 vector mask cases plus four SQI/SQD dual-issue old-Fs cases; existing VU pipeline/block/branch cases;552 COP1 complete states identical to R1348 helper baseline;56 link controls; ARM reply rejection/cache-order, session-retry, async stale epoch and completion-wakeup cases; actual shipped ARM capability handler; EABI helper tests; native prefix foreign-arena ownership; exact EE/IOP/SIF8:1 retirement.

Use the same settings as R1349 and Cold Boot. Four-minute boot and actual ARM availability are not proven. Full native JIT coverage, DSI path and zero-vertex hardware confirmation remain open.
