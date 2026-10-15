/* GPL-3.0-or-later. Tuxedo glue adapted from nullDC4Wii wii_fastmem.cpp,
 * commit 26a623e3feca9927d8ffe43706b06d82d672dd64. libogc >= 3 required. */
#if defined(GEKKO2_DSI_FASTMEM)
 .text
 .align 2
 .globl gekko2_dsi_probe_word
 .type gekko2_dsi_probe_word,@function
gekko2_dsi_probe_word:
 lwz 3,0(3)
 blr
 .size gekko2_dsi_probe_word,.-gekko2_dsi_probe_word
 .globl gekko2_dsi_entry
 .type gekko2_dsi_entry,@function
gekko2_dsi_entry:
 stwu 1,-128(1)
 stw 0,48(1)
 stw 3,52(1)
 stw 4,56(1)
 stw 5,60(1)
 stw 6,64(1)
 stw 7,68(1)
 stw 8,72(1)
 stw 9,76(1)
 stw 10,80(1)
 mflr 0
 stw 0,84(1)
 mfctr 0
 stw 0,88(1)
 mfxer 0
 stw 0,92(1)
 lis 3,__ppc_excpt_buf@ha
 lwz 3,__ppc_excpt_buf@l(3)
 bl gekko2_dsi_handle
 cmpwi 3,0
 lwz 0,92(1)
 mtxer 0
 lwz 0,88(1)
 mtctr 0
 lwz 0,84(1)
 mtlr 0
 lwz 10,80(1)
 lwz 9,76(1)
 lwz 8,72(1)
 lwz 7,68(1)
 lwz 6,64(1)
 lwz 5,60(1)
 lwz 4,56(1)
 lwz 3,52(1)
 lwz 0,48(1)
 addi 1,1,128
 bne 1f
 lis 12,__ppc_excpt_buf@ha
 la 12,__ppc_excpt_buf@l(12)
 lwz 11,0(12)
 mtsrr0 11
 lwz 11,4(12)
 mtsrr1 11
 lwz 11,8(12)
 mtcr 11
 mfsprg 11,0
 mfsprg 12,1
 rfi
1:
 li 11,3
 cmpwi 1,0
 b PPCExcptDefaultHandler
 .size gekko2_dsi_entry,.-gekko2_dsi_entry
#endif
 .section .note.GNU-stack,"",@progbits
